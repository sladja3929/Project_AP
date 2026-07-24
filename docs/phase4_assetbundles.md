# Phase 4 — AssetBundles + ChangeBundleState 승격 (Combat 몽타주)

> 동기→비동기 로드 리팩토링(plan.txt)의 마지막 단계. Phase 1~3·5는 `docs/async_load_refactor_comparison.md` 참조.
> 상태: **구현·정적검증·에디터콘솔검증 완료. 최종 쿡 검증 1회 대기.**

## 1. 무엇을 왜 했나

Phase 2에서 각 DataAsset은 몽타주를 `RequestAsyncLoad` + `TSharedPtr<FStreamableHandle>`로 **수동 프리로드**하고
핸들을 직접 보관했다. Phase 4는 **몽타주만** 언리얼 AssetManager의 **AssetBundles + ChangeBundleState**로 승격했다.

목표:

- DA 안쪽 몽타주를 "Combat" 번들로 묶어 **AssetManager가 로드/참조 관리**를 담당 → DA의 수동 핸들 로직 축소
- AssetManager를 **정석대로 설정**(PrimaryAssetTypesToScan + GetPrimaryAssetId) → 포트폴리오 가치
- 번들 단위 로드/언로드의 확장성 확보

**범위**: 몽타주(Combat)만. UI(Icon)/Visual(UseMesh)은 Phase 5 방식(RequestAsyncLoad + 데디서버 스킵) 유지.
근거: Phase 5가 이미 서버 UseMesh 스킵/UI 미생성을 달성해, UI/Visual 번들화는 실익 대비 리스크가 컸다.

## 2. 승격 대상 (Combat 번들에 들어간 몽타주)

- 공유 구조체 `FComboAttackUnit`(AttackData.h)의 `AttackMontage`, `SubAttackMontage` — Weapon/Enemy 콤보 공용
- `FBlockActionData`(WeaponDataAsset.h): BlockIdle, BlockReaction Light/Middle/Heavy, GuardBreak, Parry
- `UEnemyDataAsset`: Death, HitReaction Light/Middle/Heavy, Groggy Start/Loop/End
- `UUsableItemDataAsset::UseMontage`

## 3. 구현 상세

### Step 1 — AssetManager 설정 (`Config/DefaultGame.ini`)

`[/Script/Engine.AssetManagerSettings]` 섹션 신설, primary asset 타입 3종 등록:

```ini
+PrimaryAssetTypesToScan=(PrimaryAssetType="EnemyData",AssetBaseClass="/Script/ActionPractice.EnemyDataAsset",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game")),Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
+PrimaryAssetTypesToScan=(PrimaryAssetType="WeaponData",AssetBaseClass="/Script/ActionPractice.WeaponDataAsset",...)
+PrimaryAssetTypesToScan=(PrimaryAssetType="UsableItem",AssetBaseClass="/Script/ActionPractice.UsableItemDataAsset",...)
```

- `bHasBlueprintClasses=False`: DA .uasset 바이너리를 조사해 **네이티브 클래스 인스턴스**임을 확인(BP 서브클래스 아님).
- `Directories=/Game` + AssetBaseClass 필터로 스캔.

### Step 2 — `GetPrimaryAssetId` 오버라이드 (3 DA)

```cpp
virtual FPrimaryAssetId GetPrimaryAssetId() const override
{ return FPrimaryAssetId(TEXT("EnemyData"), GetFName()); }   //타입명은 Step 1과 정확히 일치
```

WeaponDataAsset→"WeaponData", UsableItemDataAsset→"UsableItem". 타입명 불일치 시 ChangeBundleState가 no-op이 되므로 중요.

### Step 3 — `meta=(AssetBundles="Combat")` 태깅

2절의 몽타주 소프트 포인터 UPROPERTY 전부에 meta 부착. `UPrimaryDataAsset::UpdateAssetBundleData()`가
UPROPERTY(구조체·배열 재귀 포함)를 순회해 쿡/저장 시 번들 데이터를 생성한다. UseMesh/Icon에는 **미부착**.

### Step 4 — 배치 프리로드 → `ChangeBundleStateForPrimaryAssets`

> ★ UE5.7 실제 API는 `ChangeBundleState`가 아니라 **`ChangeBundleStateForPrimaryAssets`**. (plan 문서의 함수명이 오기였고 구현 중 정정.)

```cpp
if (bPreloadRequested || !UAssetManager::IsInitialized()) return;
const FPrimaryAssetId AssetId = GetPrimaryAssetId();
if (!AssetId.IsValid()) return;
bPreloadRequested = true;
//반환 핸들을 멤버로 보관 — AssetManager 참조 유지 시맨틱에 의존하지 않고 확정적으로 붙잡는다
BundleHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
    { AssetId }, { FName(TEXT("Combat")) }, {}, /*bRemoveAllBundles*/false,
    FStreamableDelegate::CreateWeakLambda(this, [](){ /* 로깅 */ }),
    FStreamableManager::DefaultAsyncLoadPriority);
```

- 기존 `LoadedMontageCache`(TArray 하드 레퍼런스) + `bMontagesPreloaded` **완전 제거** → **번들 핸들 1개**로 축소.
- `BeginDestroy`에서 `BundleHandle` 정리.
- **UsableItemDataAsset은 혼합**: UseMontage=ChangeBundleState(→`BundleHandle`) / UseMesh=RequestAsyncLoad+`!IsRunningDedicatedServer()`(→별도 `PreloadHandle`). BeginDestroy에서 두 핸들 모두 취소.
- 호출부(`AWeapon::BeginPlay`, `CacheWeaponData`, `ItemManagerComponent` 슬롯 트리거) 시그니처 불변.

### Step 5 — C그룹 어빌리티 정리 (수동 핸들 제거의 완성)

EnemyDataAsset이 적 스폰(BeginPlay) 시 Combat 번들로 모든 적 몽타주를 로드하므로,
`EnemyDeath/Groggy/HitReactionAbility`가 `OnGiveAbility`에서 **자기 몽타주를 또 프리로드하던 것은 중복**이 됐다. 제거 내역:

- `OnGiveAbility`의 `RequestAsyncLoad` 블록, `PreloadHandle`, `bPreloadRequested`, 완료 콜백 캐싱
- `BeginDestroy` 오버라이드(프리로드 정리 전용이었음)
- `Cached*Montage` 멤버 (Death 1 / Groggy 3 / HitReaction 3)
- 기존 `[RaceWindow]` 계측 블록 전부

대신 `SetMontageToPlayTask`가 phase/level에 맞는 `EnemyData->몽타주`를 **`.Get()` + 동기 폴백**(Phase 3 패턴)으로 읽는다:

```cpp
UAnimMontage* Montage = SoftMontage.Get();
if (!Montage && !SoftMontage.IsNull())
{
    DEBUG_LOG(TEXT("[AsyncPreload] ... sync fallback: %s"), *SoftMontage.ToString());
    Montage = SoftMontage.LoadSynchronous();
}
```

동기 폴백이 안전망이라 번들 로드가 미완이어도 재생은 보장된다. 몽타주 선택 결과(어떤 몽타주를 재생하는가)는 리팩토링 전과 동일.

## 4. Groggy 좀비 타이머 회귀 (Step 5에서 발생 → 수정)

**타이머 자체는 원래부터 Groggy 설계였다.** Groggy는 Start→Loop→End 3단계이고, **Loop 단계가 `GroggyLoopDuration`초를
`GroggyLoopTimerHandle`로 대기**하다 만료되면 End로 넘어간다. 새 타이머를 넣은 게 아니다.

문제는 Step 5의 구조 변경이 만든 **새 위험**이었다:

- **이전**: `if (CachedGroggyLoopMontage) { 재생; 타이머; }` — 캐시가 non-null이면 재생이 내부에서 실패할 수 없어 타이머가 항상 안전.
- **Step 5 직후**: 캐시 제거로 `bHasLoopMontage = !GroggyLoopMontage.IsNull()`("할당 여부"만 판별)로 바뀌고,
  타이머 설정이 분기 밖으로 나와 **무조건 실행**됨. → `GroggyLoopMontage`가 할당은 됐으나 에셋 참조가 깨져
  `.Get()`·`LoadSynchronous()`가 모두 실패하면 `StartMontageWithEventsTask()` 내부가 `EndAbility()` 호출 →
  그런데도 뒤에서 타이머가 걸려 **종료된(InstancedPerActor 재사용) 인스턴스에 좀비 타이머**가 남음.

**수정**: `StartMontageWithEventsTask()` 직후 `if (!IsActive()) return;` 가드로, 몽타주 로드 실패로 이미 EndAbility된
경우 타이머 설정을 스킵. 정상 경로(로드 성공 or 몽타주 미할당)는 기존대로 타이머 설정. (좁은 데이터 무결성 엣지
케이스라 Warning 등급이었음.)

## 5. Before / After — DA 몽타주 로딩

|        | Before (Phase 2)                  | After (Phase 4)                                           |
| ------ | --------------------------------- | --------------------------------------------------------- |
| 배치 로드  | `RequestAsyncLoad(개별 소프트패스 배열)`   | `ChangeBundleStateForPrimaryAssets({AssetId},{"Combat"})` |
| GC 방지  | 없음(핸들이 유지)                        | 없음(핸들이 유지)                                                |
| 보관     | 수동 `FStreamableHandle` (에셋 배열 로드) | 번들 핸들 1개 (AssetManager 협조)                                |
| 적 어빌리티 | 각자 OnGiveAbility에서 개별 프리로드 + 캐시   | 제거 — DA 번들 로드 + `.Get()`+폴백                               |
| 대상 관리  | 코드에서 소프트패스 수집 루프                  | 에디터에서 `meta=(AssetBundles)` 선언                            |

## 6. 검증

### 완료 — 에디터 콘솔 (쿡 없이, 리스크 2·3 해소)

`AssetManager.DumpTypeSummary`:

```
EnemyData: Class EnemyDataAsset, Count 2
UsableItem: Class UsableItemDataAsset, Count 3
WeaponData: Class WeaponDataAsset, Count 5
```

`AssetManager.DumpBundlesForAsset EnemyData:DA_WoodGiant` → `Bundle: Combat (10 assets)`에 콤보 6종 + Death +
Groggy 3종. **`TArray<FComboAttackUnit>` 안의 콤보 몽타주가 번들에 정상 수집됨을 확인**(핵심 리스크 해소).
WeaponData:DA_Bastard_Sword → Combat 10종(GreatSword 콤보/차지), UsableItem:DA_Potion → Combat 1종(Drink).

### 남음 — 최종 쿡 검증 (패키지 빌드 후 1회)

1. 무기 장착/공격(콤보·차지)/블록·패리, 포션 사용, 적 전투(피격/그로기/사망) 몽타주 정상 재생
2. 전투 중 `obj gc` 후 재생 유지 (번들 핸들 보관으로 이미 안전 — 사후 확인)
3. `[AsyncPreload] ... sync fallback` 로그 **0** 확인 (뜨면 프리로드 트리거 시점 재조정)
4. 데디서버: 몽타주 로드/타이밍 정상, UseMesh/UI 미로드 유지
5. 통과 시 main 병합 (그 전까지 develop 유지)

## 7. 변경 파일

- `Config/DefaultGame.ini`
- `Public/Items/AttackData.h`
- `Public/Items/WeaponDataAsset.h`, `Private/Items/WeaponDataAsset.cpp`
- `Public/Characters/Enemy/EnemyDataAsset.h`, `Private/Characters/Enemy/EnemyDataAsset.cpp`
- `Public/Items/UsableItemDataAsset.h`, `Private/Items/UsableItemDataAsset.cpp`
- `Public/GAS/Abilities/Enemy/EnemyDeathAbility.h`, `Private/.../EnemyDeathAbility.cpp`
- `Public/GAS/Abilities/Enemy/EnemyGroggyAbility.h`, `Private/.../EnemyGroggyAbility.cpp`
- `Public/GAS/Abilities/Enemy/EnemyHitReactionAbility.h`, `Private/.../EnemyHitReactionAbility.cpp`

##  8. 포트폴리오 관점

"동기 LoadSynchronous → 핸들 기반 RequestAsyncLoad → **AssetManager AssetBundles**"라는 3단 진화가 서사가 된다.
특히 마지막 단계에서 (1) 로딩 대상 관리를 코드 수집 루프에서 **데이터(meta 선언)**로 옮기고,
(2) 어빌리티별 수동 프리로드를 **번들 로드 + 사용 시점 `.Get()`+폴백**으로 일원화했으며,
(3) `ChangeBundleStateForPrimaryAssets`의 실제 UE5.7 API를 검증하고, (4) 배열 내 구조체 meta 수집을
**쿡 없이 에디터 콘솔로 사전 검증**한 점이 실무 감각을 보여준다.
