---
name: feedback-widget-async-load-pattern
description: UMG 위젯에서 TSoftObjectPtr 텍스처를 RequestAsyncLoad로 비동기 로드할 때 콜백 수명 안전성 검증 체크리스트
metadata:
  type: feedback
---

`EquipmentSlotWidget`/`NotificationEntryWidget`의 아이콘 비동기 로드(2026-07-21, Phase 5)를 검수하며 확립된, UMG 위젯 + StreamableManager 조합에서 확인해야 할 체크리스트:

- **CreateLambda vs CreateWeakLambda 선택 기준**: `CreateWeakLambda(UObject*, ...)`는 UObject 1개만 자동 수명 체크한다. 콜백에서 위젯과 그 위젯이 소유한 하위 UObject(예: `UImage*`)처럼 **두 개 이상의 객체 수명을 체크해야 하면** `CreateLambda` + 콜백 진입부에서 `TWeakObjectPtr` 여러 개를 수동으로 `.Get()` 후 null 체크하는 패턴이 정답이다. 이 경우 CreateWeakLambda를 못 쓴 것을 결함으로 오인하지 말 것 — 의도된 선택.
- **TMap<UObject*, FRequest> 키 안전성**: 위젯의 `BindWidget` 하위 컴포넌트(예: `UImage*`)는 위젯 인스턴스 생존 기간 내내 고정된 포인터이므로(NativeConstruct 시 1회 바인딩) raw pointer를 TMap 키로 써도 stale-key 문제 없음. 단, 위젯 풀링/재사용 시나리오에서 "같은 위젯 인스턴스가 다른 데이터로 재사용"되는 것과 "위젯 자체가 파괴되고 새 인스턴스가 생성"되는 것을 구분해야 함 — 전자는 TMap이 위젯 인스턴스 소속이라 안전, 후자는 새 인스턴스가 별도의 빈 TMap을 가지므로 역시 안전.
- **`FStreamableHandle::CancelHandle()`의 엔진 보장**: 엔진 헤더 주석상 "This stops the completion callback from happening, even if it is in the delayed callback queue" — 즉 `CancelHandle()` 호출 후에는 그 핸들의 완료 콜백이 **다시는 호출되지 않음**이 보장된다. 따라서 "이전 요청 취소 후 새 요청 시작" 패턴에서는 이론상 레이스가 발생하지 않는다. 그럼에도 콜백 내부에 Path/세대 비교 같은 추가 방어 로직이 있다면 이는 과잉이 아니라 안전망으로 인정할 것(엔진 동작에 대한 100% 신뢰보다 명시적 방어를 선호하는 것은 합리적 선택).
- **NativeDestruct에서 전체 정리**: 위젯이 여러 개의 진행 중 요청을 가질 수 있으면(TMap 등) `NativeDestruct`에서 반드시 전체 순회하며 `CancelHandle()` 해야 한다. 단일 핸들 멤버만 있는 위젯(NotificationEntryWidget처럼 아이콘 1개)은 단일 취소로 충분.
- **AssetManager 미초기화 동기 폴백**: 위젯의 첫 프레임 등 `UAssetManager::IsInitialized()`가 false일 수 있는 상황에서, 동기 폴백(`LoadSynchronous()`)이 있는지, 없다면 최소한 안전하게 숨김/빈 상태로 폴백하는지 확인. 조용히 무한 로딩 상태로 남는 경로가 없어야 함.

**Why:** UMG 위젯은 GAS 어빌리티/DataAsset과 달리 소유 주체가 명확하지 않고(부모 위젯 파괴, 슬레이트 GC 타이밍 등) 파괴 타이밍이 더 예측하기 어려워서, 콜백 수명 검증을 어빌리티/DA보다 더 엄격하게 봐야 한다. [[project_async_load_refactor]]의 async 리팩토링 후속 리뷰에서 이 체크리스트를 재사용한다.

**How to apply:** 위젯에서 `RequestAsyncLoad` + 완료 콜백을 검수할 때 위 5개 항목을 순서대로 확인. 특히 CreateLambda(WeakLambda 아님) 사용을 발견해도 곧바로 결함으로 보지 말고, 콜백 내부에 수동 TWeakObjectPtr 체크가 있는지부터 확인할 것.
