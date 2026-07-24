---
name: hitdetection-lifecycle-parity
description: AttackTraceComponent/WeaponCCDComponent/EnemyAttackComponent 계열 히트디텍션 컴포넌트의 라이프사이클 계약(bIsPrepared, 언바인딩 시점, ValidateHit 중복판정) 정렬 검수 체크리스트
metadata:
  type: feedback
---

`WeaponCCDComponent`를 CCD 보간 방식에서 순수 `OnComponentBeginOverlap` 방식으로 개편하며 `AttackTraceComponent`(수정 금지 기준 파일)와 라이프사이클 계약을 맞춘 변경(plan.txt 기반)을 검수하며 확인한 정상 패턴:

- **HandleHitDetectionEnd**: `bIsPrepared`를 여기서 false로 만들면 안 됨. 프레임 드랍으로 HitDetectionEnd 이벤트가 HitDetectionStart보다 먼저 도착할 수 있어, `bIsPrepared`는 오직 다음 `PrepareHitDetection`에서만 갱신되어야 함. End 핸들러는 `SetCollisionEnabled(NoCollision)` + `bIsDetecting=false` + Tick off만 수행.
- **언바인딩 시점**: ASC `GenericGameplayEventCallbacks` 구독 해제(`UnbindEventCallbacks`)는 반드시 `EndPlay`에서만 호출. `HandleHitDetectionEnd`에서 호출하면 다음 공격 페이즈(다음 콤보/재장착 전)의 이벤트를 못 받는 버그가 생김.
- **ValidateHit 계약(TMap 기반 중복판정)**: `TMap<AActor*, FHitValidationData>` 사용, non-multihit이면 기존 엔트리 존재 시 즉시 false, multihit이면 `HitCooldownTime` 경과 체크 후 `LastHitTime`/`HitCount` 갱신, 신규 액터면 엔트리 추가. 이 프로젝트의 실제 공격 트레이스(`PerformSlashTrace`)는 항상 `ValidateHit(..., false)`로 하드코딩 호출하고 `PerformPierceTrace`/`PerformStrikeTrace`는 빈 스텁이라, "multi-hit 데이터 소스 없음 → 항상 false로 미러링"은 현재 코드베이스에서 정확한 구현임 (실제 소스를 grep으로 재확인 후 검수 통과 처리함, 향후 멀티히트 필드가 `FAttackStats`에 추가되면 이 미러링도 같이 갱신 필요).
- **HasAuthority 가드**: 오버랩/트레이스 콜백 진입부에서 `GetOwner()->HasAuthority()` 체크 유지되는지 항상 확인 (서버 전용 판정 정책).
- **컴포넌트 부착**: 무기 하위 히트디텍션 컴포넌트(캡슐 등)는 생성자에서 `SetupAttachment(WeaponMesh)`로 부착해야 에디터 프리뷰/BP 트랜스폼 조정이 가능함. `BeginPlay`에서 별도로 `AttachToComponent(..., KeepRelativeTransform)`를 다시 호출하는 기존 코드가 있어도, 생성자에서 이미 부착되어 있으면 단순히 같은 부모로 재부착하는 것이라 무해함 (제거를 요구할 필요는 없음, Suggestion 수준).

**Why:** `AttackTraceComponent`/`WeaponAttackComponent`/`EnemyAttackComponent`는 plan.txt에서 수정 금지로 자주 지정되는 "정답 구현" 기준 파일이며, 신규/병렬 컴포넌트(`WeaponCCDComponent` 등)를 이 계약에 맞출 때 개발자가 부분적으로만 정렬(예: bIsPrepared는 안 건드리지만 언바인딩 시점은 놓침)하는 실수가 나올 수 있다. 리뷰 시 "무엇을 건드리지 않았는가"까지 diff에서 명시적으로 확인해야 한다.

**How to apply:** 히트디텍션 계열 컴포넌트(현재 3종: AttackTraceComponent, WeaponAttackComponent, EnemyAttackComponent, 그리고 병렬 구현체 WeaponCCDComponent) 중 하나를 새로 만들거나 개편하는 diff를 볼 때마다 위 4개 항목(bIsPrepared 비접촉, EndPlay 전용 언바인딩, ValidateHit TMap 계약, HasAuthority 가드)을 체크리스트로 대조할 것. 새로 TMap<AActor*, FHitValidationData>를 재사용하는 경우 include 순환 참조 여부도 함께 확인.
