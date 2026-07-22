---
name: gas-async-preload-pattern
description: OnGiveAbility에서 TSoftObjectPtr 몽타주를 RequestAsyncLoad로 프리로드할 때 검증된 정상 패턴과 체크리스트
metadata:
  type: feedback
---

`EnemyDeathAbility`/`EnemyGroggyAbility`/`EnemyHitReactionAbility`에 적용된 OnGiveAbility 비동기 프리로드 패턴을 검수하며 확인한 정상 구조:

- `TSharedPtr<FStreamableHandle> PreloadHandle` + `bool bPreloadRequested` 를 UPROPERTY 아닌 순수 C++ 멤버로 선언 (핸들 자체가 GC로부터의 하드 레퍼런스 역할)
- `FStreamableDelegate::CreateWeakLambda(this, [this, SoftPtr...]() { Cached... = SoftPtr.Get(); })` — `this`가 UGameplayAbility(UObject 파생)이므로 안전. 단 어빌리티 `InstancingPolicy`가 반드시 `InstancedPerActor`여야 캡처된 `this`가 액터별로 분리됨 (NonInstanced/InstancedPerExecution이면 CDO나 다른 인스턴스와 캐시 필드를 공유하게 되어 크로스 액터 오염 위험 — 검수 시 InstancingPolicy를 항상 같이 확인할 것)
- `BeginDestroy()` 오버라이드에서 `PreloadHandle->CancelHandle(); PreloadHandle.Reset();` 후 반드시 `Super::BeginDestroy()` 호출
- Null 소프트 포인터의 `.Get()`은 nullptr을 반환하므로 완료 콜백에서 기존 null-guard 로직과 동일하게 동작함 (별도 분기 불필요)

**알려진 트레이드오프 (Critical 아님, 채택된 리스크):** OnGiveAbility 시점에 프리로드를 "시작"만 하고 완료를 기다리지 않으므로, 완료 전에 어빌리티가 Activate되면 캐시된 몽타주가 여전히 null이라 몽타주 없음 폴백 경로로 빠짐. 기존 LoadSynchronous는 이 경로를 원천 차단했었음 — 이는 실제 회귀 지점이지만 [[project_async_load_refactor]]에 정리된 대로 plan상 Phase 3(동기 폴백)에서 다루기로 이미 계획된 것이므로, 그 전 단계 리뷰에서는 Warning으로만 지적한다.

**Why:** 리뷰 중 InstancingPolicy를 확인하지 않으면 CreateWeakLambda가 CDO를 캡처하는 잠재적 버그를 놓칠 수 있고, 회귀 트레이드오프를 Critical로 잘못 격상시키면 이미 계획된 리스크를 재차 지적하는 노이즈가 된다.

**How to apply:** 같은 패턴이 EnemyDataAsset/UsableItemDataAsset/WeaponDataAsset의 Phase 2 전환(다음 단계)에 적용될 때도 동일 체크리스트(핸들 멤버, InstancingPolicy, BeginDestroy Super 호출, null-guard 동등성)로 검수할 것.

**추가 발견 (race-window 계측 로그의 다중 슬롯 오탐):** developer가 race window 감지용 DEBUG_LOG를 "`bPreloadRequested == true`인데 캐시가 null이면 race"로 판정하는 방식을 추가했을 때, 몽타주 슬롯이 하나뿐인 어빌리티(EnemyDeathAbility - DeathMontage 1개)는 정확하지만, 슬롯이 여러 개인 어빌리티(EnemyGroggyAbility - Start/Loop/End 3개, EnemyHitReactionAbility - Light/Middle/Heavy 3개)는 **어빌리티 전체에 대해 `bPreloadRequested` 플래그가 하나뿐**이라 오탐이 발생함: 슬롯 중 하나라도 존재하면 플래그가 true가 되고, 그 상태에서 **다른 슬롯이 디자인상 원래 비어있는 정상 케이스**(예: Groggy Loop 몽타주 없이 타이머만 쓰는 기존 지원 패턴)도 "race window"로 잘못 로깅됨. 슬롯 단위 판정(개별 SoftObjectPtr의 IsNull() 체크, 또는 슬롯별 bool)이 필요함.

**How to apply:** 다중 슬롯을 가진 어빌리티/DA에 유사한 race-window 계측을 추가할 때는 항상 "플래그가 어빌리티 단위인지 슬롯 단위인지"를 확인하고, 단위가 맞지 않으면 Warning으로 지적할 것.

**후속 확인 (2026-07-21, 슬롯 단위 판정으로 수정 완료됨):** EnemyDeathAbility/EnemyGroggyAbility/EnemyHitReactionAbility 전부 `bPreloadRequested` 대신 `EnemyData->XxxMontage.IsNull()` 슬롯별 판정으로 교체되어 다중 슬롯 오탐 문제는 해결됨. 단, 이 수정 과정에서 새로운 패턴의 결함을 발견함:

**도달 불가능한(dead code) 슬롯 계측 — EnemyGroggyAbility Loop 슬롯 사례:** `StartMontageWithEventsTask()` 내부에 슬롯별 race-window 로그를 넣어도, 그 함수를 호출하는 상위 로직이 "몽타주가 없으면 아예 호출하지 않고 다른 경로로 우회"하는 조건부 스킵을 갖고 있으면 해당 슬롯의 로그는 영원히 찍히지 않는다. 구체적으로 `EnemyGroggyAbility::StartGroggyLoop()`은 `if (CachedGroggyLoopMontage) { StartMontageWithEventsTask(); } else { /*타이머만 설정, StartMontageWithEventsTask 호출 안 함*/ }` 구조라서, Loop 몽타주가 (디자인상 없든 / race로 아직 로드 안 됐든) null이면 `StartMontageWithEventsTask()` 자체가 호출되지 않아 그 안의 `case EGroggyPhase::Loop` 로그 분기가 도달 불능이 된다. Start/End phase는 상위에서 무조건 `StartMontageWithEventsTask()`를 호출하므로 문제없이 로그가 찍힌다.

**Why:** 계측 코드를 추가할 때 "이 함수가 null 몽타주 케이스에서 항상 호출되는가"를 확인하지 않으면, 슬롯 단위로 조건을 정확히 나눠도 상위 제어 흐름의 조기 분기 때문에 로그가 침묵하는 그림자 케이스가 생긴다. 기능적으로는 기존 폴백(타이머만 진행)과 동일하게 동작해 크래시나 오동작은 없지만, 계측을 추가한 목적(진짜 race와 디자인상 빈 슬롯을 구분해 로그로 드러내기)을 그 슬롯에서만 달성하지 못한다.

**How to apply:** 멀티페이즈/멀티슬롯 어빌리티에 슬롯별 race-window 로그를 검수할 때는, 로그가 위치한 함수가 "몽타주 null인 모든 케이스"에서 실제로 호출되는지 호출부(상위 State/Phase 전환 로직)까지 추적할 것. 조건부로 그 함수 호출 자체를 건너뛰는 경로가 있으면 그 슬롯의 로그는 Warning으로 지적 (dead code / 계측 사각지대).

**수정 확인 (2026-07-21):** `EnemyGroggyAbility::StartGroggyLoop()`의 `else` 분기(공용 `StartMontageWithEventsTask` null-guard를 타지 않는 유일한 경로)에 직접 슬롯 판정 로그를 추가하는 방식으로 해결됨 — 이 분기는 `CachedGroggyLoopMontage`가 null일 때 항상 실행되므로 dead code 문제 해소. 단 `StartMontageWithEventsTask()` 내부 스위치의 `case EGroggyPhase::Loop`는 여전히 도달 불가능한 채로 남아있음(기능적 문제는 아니고 중복 로깅도 없음 — 정리하면 좋지만 필수는 아닌 Suggestion 수준). 이런 "우회 경로가 여러 개인 함수"에 계측을 추가할 때는 공용 함수 안에 한 곳만 계측하지 말고, 그 함수를 우회하는 모든 호출부까지 계측이 커버하는지 확인하는 습관이 유효함을 재확인.
