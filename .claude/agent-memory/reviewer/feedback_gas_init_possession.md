---
name: GAS Init Possession Timing Pattern
description: UE5.7에서 PossessedBy/OnRep_Owner 기반 GAS 초기화 시 SetOwner 중복 호출 및 호출 순서 이슈
type: feedback
---

UE5.7 엔진 소스 확인 결과:
- `AController::OnPossess` 내부에서 `InPawn->PossessedBy(this)`를 먼저 호출하고, 그 다음 `SetPawn(InPawn)`을 호출함
- `APawn::PossessedBy(AController*)` 내부 첫 줄에서 이미 `SetOwner(NewController)`를 호출함

**Why:** PlayerController::OnPossess에서 Super 직후 명시적으로 `InPawn->SetOwner(this)`를 다시 호출하는 코드는,
`Super::OnPossess → PossessedBy(엔진 기본 구현) → SetOwner` 경로로 이미 처리되므로 이중 호출이 됨.
단, AActionPracticeCharacter::PossessedBy가 오버라이드되어 있고 `Super::PossessedBy(NewController)`를 호출하므로
엔진의 SetOwner는 그 Super 호출 시 실행됨.
따라서 OnPossess의 명시적 SetOwner 호출 시점은 PossessedBy(및 GAS 초기화)가 이미 완료된 이후임.

**How to apply:** 차후 검수 시 OnPossess에서의 SetOwner 명시 호출이 엔진 흐름상 중복(무해하지만)임을 지적할 것.
GAS Mixed 모드 정합성 목적으로 넣었다면, 실제 SetOwner는 Super::PossessedBy에서 이미 이루어지므로
OnPossess의 guard 조건(`if InPawn->GetOwner() != this`)은 항상 false가 되어 실행되지 않음.
