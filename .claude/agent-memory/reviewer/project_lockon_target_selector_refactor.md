---
name: lockon-target-selector-refactor
description: plan.txt "락온 타겟 탐색/전환 고도화" 1~8단계 구현 검수 결과와 승인된 설계 이탈 목록 (2026-09-21)
metadata:
  type: project
---

`D:\unreal\ActionPractice\plan.txt`의 락온 타겟 탐색/전환 고도화 계획(1~8단계) 전체가 구현되어 2026-09-21에 검수, VERDICT: OK로 통과.

**검증 방법:** Editor Development 빌드(공식 빌드 명령) 성공, `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests ActionPractice.LockOn; Quit" -nullrhi` 로 신규 Automation Test 4종(ProjectToCameraPlane/ScoreCandidate/BuildRankedHeap/SelectSwitchTarget) 전부 Success 확인.

**최종 구조:**
- `Characters/LockOn/LockOnTargetSelector.h/.cpp` — 월드/액터/물리 의존 없는 순수 정적 함수 계층(`FLockOnView`, `FLockOnCandidate`, `FLockOnScoringParams`). Actor 참조는 `TWeakObjectPtr`만 사용.
- `LockOnComponent`가 물리 질의(`OverlapMultiByObjectType` ECC_Pawn / `LineTraceTestByObjectType` ECC_WorldStatic+WorldDynamic)로 후보를 모으고 Selector에 위임.
- 모든 탐색/전환/유지 진입점(`ToggleLockOn`, `FindBestTarget`, `AccumulateSwitchInput`, `StartLockOnMaintenance`, `TickLockOnMaintenance`, `HandleTargetLost`)이 `IsLocallyControlledOwner()` 가드로 시작 — 데디서버·리모트 클라이언트 진입 차단.

**승인된 plan.txt 대비 설계 이탈(문제 아님, 다음 검수에서 지적하지 말 것):**
- `BuildLockOnView`를 `bool BuildLockOnView(FLockOnView& OutView) const`로 (plan은 값 반환 시그니처를 제안).
- `IsTargetVisible(View, const FVector& AimPoint)`로 단순화 (plan은 `FLockOnCandidate` 전체를 받는 시그니처 제안).
- `PopRankedCandidate` 래퍼 추가, 내부에서 `HeapPop(..., EAllowShrinking::No)` 사용 확인됨(5.7 시그니처 일치).
- `SwitchVerticalInputScale` 기본값 0.0 (좌우 전환만 사용, IA_Look Y 부호가 프로젝트 인풋 모디파이어에 의존적이라는 이유를 헤더 주석에 명시).
- 서버 검증 실패 시 클라이언트 보정용 Client RPC 미구현 — "오탐 시 조작감 저해"를 이유로 주석에 명시, plan도 구현 여부를 판단에 맡김.

**알려진 트레이드오프(설계상 수용, 결함 아님):** `ServerSetLockOnState_Implementation`이 검증 실패 시 서버 상태를 갱신하지 않지만 클라이언트는 이미 로컬에서 `bIsLockOn=true`로 낙관적 적용한 상태라 클라(시각)-서버(권위) 상태가 일시적으로 갈라질 수 있음. 다만 `bIsLockOn`/`GetLockOnTarget()`을 참조하는 지점(Strafe 이동 방향, 캐릭터 회전, 사망 시 로컬 해제)이 모두 로컬 실행 경로라 서버측 전투 판정에는 영향 없음을 호출부 추적으로 확인함.

**Why:** 이 문서(plan.txt)가 삭제/수정되기 전까지는 이 refactor의 유일한 스펙 소스였으므로, 승인된 이탈 목록을 기억해두지 않으면 다음 검수에서 동일 항목을 다시 이슈로 올리는 노이즈가 생김.

**How to apply:** `LockOnComponent`/`LockOnTargetSelector` 관련 후속 diff를 검수할 때 위 이탈 목록은 재차 지적하지 말 것. 새로운 변경이 이 트레이드오프(서버 보정 RPC 없음, SwitchVerticalInputScale=0)를 뒤집는지만 확인. 관련 일반 패턴은 [[feedback_camera_local_only_pattern]] 참고.
