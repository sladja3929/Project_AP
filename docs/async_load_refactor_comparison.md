# 동기 → 비동기 에셋 로드 리팩토링: 전/후 비교

> 기준: `git diff HEAD` (리팩토링 전 = HEAD, 후 = 현재 작업 트리)
> 범위: plan.txt Phase 1 ~ 3, 5 (Phase 4 AssetBundles 승격은 보류)
> 규모: 소스 23개 파일 수정 + 2개 파일 신규(`EnemyDataAsset.cpp`, `WeaponDataAsset.cpp`), 약 +692 / −264 라인

---

## 1. 한눈에 보는 변화

| 구분 | Before (동기) | After (비동기) |
|---|---|---|
| DA 배치 프리로드 | `StreamableManager.LoadSynchronous()` 루프 (블로킹) | `RequestAsyncLoad()` 1회 배치 (논블로킹) |
| GC 방지 | `UPROPERTY(Transient) TArray<TObjectPtr<UAnimMontage>> LoadedMontageCache` 하드 레퍼런스 | 반환된 `TSharedPtr<FStreamableHandle>` 자체가 하드 레퍼런스 |
| 중복 방지 | `bMontagesPreloaded` / `bAssetsPreloaded` (UPROPERTY) | `bPreloadRequested` (순수 C++ bool) |
| 무기 몽타주 로드 트리거 | 게터(`GetWeaponBlockData` 등) 내부 lazy 로드 | `AWeapon::BeginPlay`(장착 시점) 선행 |
| 사용 시점 로드 | `SoftMontage.LoadSynchronous()` (매번 블로킹 가능) | `.Get()` 우선 + 조건부 동기 폴백(결정론 안전망) |
| 적 어빌리티 프리로드 | `OnGiveAbility`에서 `LoadSynchronous` 즉시 캐싱 | `OnGiveAbility`에서 `RequestAsyncLoad` + 완료 콜백 캐싱 |
| UI 아이콘 로드 | `Icon.LoadSynchronous()` (블로킹) | `RequestAsyncLoad` + 완료 콜백 `SetBrushFromTexture` |
| 데디서버 시각 에셋 | 무조건 로드 | UseMesh/UI 텍스처 스킵 (몽타주는 유지) |
| 핸들 수명 정리 | 없음 | `BeginDestroy`/`NativeDestruct`에서 `CancelHandle()` |

핵심 목표: **hitch(프레임 스파이크) 제거**. 몽타주/텍스처를 "필요 수 초 전"에 백그라운드로 미리 로드해두고, 사용 시점엔 이미 메모리에 있는 것을 `.Get()`으로 즉시 집는다.

---

## 2. 핵심 패턴 Before / After

### 2-1. DA 배치 프리로드 (EnemyDataAsset / WeaponDataAsset / UsableItemDataAsset)

**Before** — 헤더 인라인, 동기 블로킹 + 하드 레퍼런스 캐시
```cpp
void PreloadAllMontages()
{
    if (bMontagesPreloaded) return;
    TArray<FSoftObjectPath> AssetsToLoad;
    // ... 대상 수집 ...
    if (AssetsToLoad.Num() > 0 && UAssetManager::IsInitialized())
    {
        FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
        LoadedMontageCache.Reset(AssetsToLoad.Num());
        for (const FSoftObjectPath& AssetPath : AssetsToLoad)
        {
            //  ↓ 프레임을 멈추고 디스크에서 즉시 로드 (hitch 원인)
            if (UObject* Loaded = StreamableManager.LoadSynchronous(AssetPath))
                if (UAnimMontage* M = Cast<UAnimMontage>(Loaded))
                    LoadedMontageCache.Add(M);   // GC 방지용 하드 레퍼런스
        }
    }
    bMontagesPreloaded = true;
}

UPROPERTY(Transient) TArray<TObjectPtr<UAnimMontage>> LoadedMontageCache;
UPROPERTY(Transient) bool bMontagesPreloaded = false;
```

**After** — .cpp 분리, 비동기 배치 + 핸들이 곧 하드 레퍼런스
```cpp
void UEnemyDataAsset::PreloadAllMontages()
{
    if (bPreloadRequested) return;
    TArray<FSoftObjectPath> AssetsToLoad;
    // ... 대상 수집 (동일) ...
    if (AssetsToLoad.Num() == 0 || !UAssetManager::IsInitialized()) return;

    bPreloadRequested = true;
    FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
    //  ↓ 프레임을 멈추지 않고 백그라운드 로드 요청
    PreloadHandle = StreamableManager.RequestAsyncLoad(
        AssetsToLoad,
        FStreamableDelegate::CreateWeakLambda(this, [NumAssets]() { /* 핸들만 유지 */ }),
        FStreamableManager::DefaultAsyncLoadPriority, false, false,
        FString(TEXT("UEnemyDataAsset Montages")));   // Insights 추적용 DebugName
}

void UEnemyDataAsset::BeginDestroy()
{
    if (PreloadHandle.IsValid()) { PreloadHandle->CancelHandle(); PreloadHandle.Reset(); }
    Super::BeginDestroy();
}

//  핸들 자체가 로드 에셋을 붙잡으므로 TArray 캐시 불필요
TSharedPtr<FStreamableHandle> PreloadHandle;   // UPROPERTY 아님
bool bPreloadRequested = false;
```

핵심 인사이트: `RequestAsyncLoad`가 돌려주는 `FStreamableHandle`은 **살아있는 동안 대상 에셋을 GC로부터 지켜준다.** 따라서 기존에 GC 방지용으로 두었던 `TArray<TObjectPtr>` 하드 레퍼런스 캐시가 통째로 사라졌다. (DA 자체는 소유 액터 — `AWeapon::WeaponData`, `AEnemyCharacter::EnemyData` — 가 하드 레퍼런스로 잡고 있어 핸들 수명이 보장됨.)

### 2-2. 사용 시점 로드 (Phase 3 — B그룹 어빌리티)

**Before** — 매 사용마다 동기 로드 (프리로드가 안 끝났으면 블로킹)
```cpp
UAnimMontage* Montage = ComboData.AttackMontage.LoadSynchronous();
```

**After** — `.Get()` 우선 + 조건부 동기 폴백
```cpp
//프리로드 완료분은 .Get()으로 즉시 획득(사실상 no-op), 미완료 시에만 동기 폴백
//폴백은 비동기 전환 실패가 아니라 입력 반응성 + 데디서버 코옵 결정론을 위해 의도적으로 남긴 안전망이다
//폴백 로그가 뜨는 지점 = 프리로드 트리거 배치가 잘못된 지점 (목표는 로그가 한 번도 안 뜨는 상태)
UAnimMontage* Montage = ComboData.AttackMontage.Get();

if (!Montage && !ComboData.AttackMontage.IsNull())
{
    DEBUG_LOG(TEXT("[AsyncPreload] AttackSequence AttackMontage not preloaded, sync fallback: %s"),
              *ComboData.AttackMontage.ToString());
    Montage = ComboData.AttackMontage.LoadSynchronous();
}
```

설계 의도: 몽타주는 **그 프레임에 반드시** 필요하다(입력 반응성/네트워크 결정론). 비동기 대기로 재생을 미루면 코옵에서 클라마다 타이밍이 어긋난다. 그래서 폴백을 남기되, 폴백이 실행되면 `[AsyncPreload]` 경고 로그로 "프리로드 트리거 배치가 잘못된 지점"을 추적한다. **폴백 제거가 목표가 아니라 "폴백 로그가 0"이 목표.**

### 2-3. 적 어빌리티 OnGiveAbility 프리로드 (Phase 2 C그룹)

**Before**
```cpp
if (EnemyData && !EnemyData->DeathMontage.IsNull())
    CachedDeathMontage = EnemyData->DeathMontage.LoadSynchronous();  // 부여 시점 블로킹
```

**After** — 부여 시점엔 요청만, 완료 콜백에서 캐싱 (부여~사용까지 수 초 여유 활용)
```cpp
if (Enemy && !bPreloadRequested && UAssetManager::IsInitialized())
{
    // ...
    bPreloadRequested = true;
    PreloadHandle = StreamableManager.RequestAsyncLoad(
        DeathMontage.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(this, [this, DeathMontage]()
        {
            CachedDeathMontage = DeathMontage.Get();   // 완료 시 사용 포인터 반영
        }),
        /* ... */ FString(TEXT("UEnemyDeathAbility DeathMontage")));
}
```
`InstancedPerActor` 인스턴싱이라 `CreateWeakLambda(this, ...)` 캡처가 액터별로 안전하게 분리됨. `BeginDestroy`에서 `CancelHandle`.

### 2-4. 계측 — race window 슬롯 단위 판정 (선결 과제)

다중 슬롯 어빌리티(Groggy: Start/Loop/End, HitReaction: Light/Middle/Heavy)에서, 어빌리티 단위 `bPreloadRequested`로 판정하면 **디자인상 원래 비어있는 슬롯**이 race로 오탐된다. → 사용 시점에 캐시가 null일 때, **해당 슬롯의 원본 `TSoftObjectPtr.IsNull()`을 슬롯 단위로 확인**해서 "지정됐는데 캐시 null"일 때만 `[RaceWindow]` 로그.

```cpp
bool bSlotExpected = false;
switch (CurrentPhase)
{
case EGroggyPhase::Start: bSlotExpected = !EnemyData->GroggyStartMontage.IsNull(); break;
case EGroggyPhase::Loop:  bSlotExpected = !EnemyData->GroggyLoopMontage.IsNull();  break;
case EGroggyPhase::End:   bSlotExpected = !EnemyData->GroggyEndMontage.IsNull();   break;
}
if (bSlotExpected)
    DEBUG_LOG(TEXT("[RaceWindow] Groggy montage preload not complete at use (Phase=%d)"), (int32)CurrentPhase);
```
(Groggy Loop는 `StartGroggyLoop()` else 분기가 공용 null-guard 경로를 안 타므로, 그 분기에도 별도 슬롯 계측을 직접 추가 — 계측 사각지대 제거.)

### 2-5. UI 텍스처 비동기 + 위젯 수명 안전 (Phase 5)

**Before**
```cpp
UTexture2D* IconTexture = ItemDA->Icon.LoadSynchronous();  // 블로킹
if (IconTexture) TargetImage->SetBrushFromTexture(IconTexture);
```

**After** — 로드 중 빈 슬롯 표시 → 완료 콜백에서 반영, 파괴/재사용 안전
```cpp
CancelPendingIconLoad(TargetImage);        // 이전 요청 취소
ApplyEmptySlotIcon(TargetImage);           // 로드 중 빈 텍스처
// ...
TWeakObjectPtr<UEquipmentSlotWidget> WeakThis(this);
TWeakObjectPtr<UImage> WeakImage(TargetImage);
Handle = StreamableManager.RequestAsyncLoad(IconPath,
    FStreamableDelegate::CreateLambda([WeakThis, WeakImage, SoftIcon, IconPath]()
    {
        UEquipmentSlotWidget* StrongThis = WeakThis.Get();
        UImage* Image = WeakImage.Get();
        if (!StrongThis || !Image) return;                       // 파괴 안전
        const FSlotIconRequest* Latest = StrongThis->PendingIconRequests.Find(Image);
        if (!Latest || Latest->Path != IconPath) return;         // 세대 검증(슬롯 재사용 안전)
        if (UTexture2D* Tex = SoftIcon.Get())
        {
            Image->SetBrushFromTexture(Tex);
            Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        StrongThis->PendingIconRequests.Remove(Image);
    }), FStreamableManager::DefaultAsyncLoadPriority);
```
- `NativeDestruct`에서 진행 중 모든 핸들 `CancelHandle` (`UImage` 두 객체 생존을 모두 봐야 해 `CreateWeakLambda` 대신 `TWeakObjectPtr` 수동 캡처).
- `AssetManager` 미초기화 시 동기 폴백 유지(무한 빈 텍스처 방지).

---

## 3. Phase별 파일 매핑

### Phase 1 — 로드 트리거 앞당기기 (동기 유지)
| 파일 | 변화 |
|---|---|
| `Items/Weapon.cpp` | `GetWeaponBlockData`/`GetWeaponAttackDataByTag` 게터 내부 lazy 로드 제거 → `BeginPlay`에서 `PreloadAllMontages()` 선행. `#include "Animation/AnimMontage.h"` 제거 |
| `Characters/ItemManagerComponent.cpp/.h` | `PreloadEquippedItemAssets()` 신설, BeginPlay(서버)/CycleQuickSlot/OnRep_Slots/OnRep_EquippedIndex에서 호출 |
| `Items/UsableItemDataAsset` | `PreloadMontage()` → `PreloadAssets()`로 확장(UseMesh StaticMesh 포함) |

### Phase 2 — 배치 프리로드 async 전환
| 파일 | 변화 |
|---|---|
| `Characters/Enemy/EnemyDataAsset.h` + `.cpp`(신규) | 헤더 인라인 동기 → .cpp 비동기. `LoadedMontageCache`/`bMontagesPreloaded` 제거 → `PreloadHandle`+`bPreloadRequested`, `BeginDestroy` |
| `Items/UsableItemDataAsset.h/.cpp` | 동일 패턴. `LoadedAssetCache`/`bAssetsPreloaded` 제거 |
| `Items/WeaponDataAsset.h` + `.cpp`(신규) | 동일 패턴. 콤보/차지/블록/패리 몽타주 전체 유지 |
| `GAS/Abilities/Enemy/EnemyDeath·Groggy·HitReactionAbility.h/.cpp` (C그룹) | `OnGiveAbility` `LoadSynchronous` → `RequestAsyncLoad`+완료 콜백 캐싱, `BeginDestroy` CancelHandle, 슬롯 단위 race 계측 |

### Phase 3 — 사용 시점 `.Get()` + 동기 폴백
| 파일 | 전환 지점 |
|---|---|
| `Player/AttackSequenceAbility.cpp` | AttackMontage, SubAttackMontage |
| `Enemy/EnemyAttackAbility.cpp` | ComboData.AttackMontage |
| `Player/BlockAbility.cpp` | BlockIdleMontage |
| `Player/HitReactionAbility.cpp` | GuardBreak / BlockReaction(Heavy fallback) / 레벨별 BlockReaction |
| `Player/ParryAbility.cpp` | ParryMontage |
| `Player/UseItemAbility.cpp` | UseMontage, UseMesh |

### Phase 5 — 서버/클라 분기 + UI 텍스처 비동기
| 파일 | 변화 |
|---|---|
| `UI/EquipmentSlotWidget.h/.cpp` | 이미지별 `TMap<UImage*, FSlotIconRequest{Handle,Path}>` 관리, 세대 검증, `NativeDestruct` 전체 취소, `CancelPendingIconLoad`/`ApplyEmptySlotIcon` 신설 |
| `UI/NotificationEntryWidget.h/.cpp` | 단일 `IconLoadHandle`, `NativeDestruct` 신설 |
| `Items/UsableItemDataAsset.cpp` | `PreloadAssets`에서 UseMesh만 `!IsRunningDedicatedServer()` 조건부 (UseMontage는 서버 유지) |
| `Player/UseItemAbility.cpp` | `SpawnItemMesh` 초입 `if (IsRunningDedicatedServer()) return;` |

---

## 4. 제외/보류 (의도적)

- **Phase 4 (AssetBundles + `ChangeBundleState`)**: `PrimaryAssetTypesToScan` 미등록 상태라 인프라 선행 작업 필요. 서버 메모리 절감 실익은 크지만 리스크가 있어 별도 판단 대상으로 보류.
- **데드 코드**: `BaseAttackAbility`/`NormalAttackAbility`/`ChargeAttackAbility` 계열(AttackSequenceAbility로 대체됨), `AbilityTask_PlayNormalAttackMontage`(생성 호출부 없음) — Phase 3 전환 대상에서 제외.
- **`TitleScreenWidget`의 `TSoftObjectPtr<UWorld>`**: `GetLongPackageName()`만 쓰고 `OpenLevel`로 넘기는 케이스라 로드하면 안 됨 — 손대지 않음.
- **몽타주 서버 로드**: 노티파이 타임라인/몽타주 길이 기반 어빌리티 종료에 필요하므로 데디서버에서도 유지(스킵 대상은 UseMesh/UI 텍스처 같은 순수 시각 에셋만).

---

## 5. 포트폴리오 관점 서사

1. **문제 진단**: 무기 장착/적 스폰/아이템 사용 프레임에서 `LoadSynchronous`로 인한 hitch. 그리고 핸들 없는 `LoadSynchronous`는 반환 직후 GC 대상이 되고 `TSoftObjectPtr`도 WeakObjectPtr 기반이라 하드 레퍼런스를 유지 못 함.
2. **1차 대응(과도기)**: `TArray<TObjectPtr>` 하드 레퍼런스 캐시로 GC 방지 → 동작은 하나 여전히 동기 블로킹.
3. **최종 구조**: `RequestAsyncLoad` + `FStreamableHandle`로 전환. **핸들이 곧 하드 레퍼런스**라는 점을 이용해 캐시를 제거하고, 로드 타이밍을 "사용 수 초 전"으로 앞당김.
4. **결정론 트레이드오프의 의식적 설계**: 사용 시점 동기 폴백은 "비동기 전환 실패"가 아니라 **입력 반응성 + 데디서버 코옵 결정론을 위해 의도적으로 남긴 안전망**. `[AsyncPreload]` 로그를 디버깅 지표로 삼아 "폴백 0"을 목표로 함.

---

## 6. 남은 검증 (코드 아님)

- **쿡 빌드**에서 검증(에디터 PIE는 에셋이 이미 로드돼 있어 문제가 안 드러남).
- Unreal Insights로 무기 장착/적 스폰/아이템 사용 프레임 hitch 전후 비교.
- `[AsyncPreload]` / `[RaceWindow]` 폴백 로그 카운트가 0인지 확인 → 0이 아니면 Phase 1 트리거 배치 재조정.
