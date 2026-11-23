#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "EnterCombatTask.generated.h"

class AEnemyAIController;

USTRUCT()
struct FEnterCombatTaskInstanceData
{
	GENERATED_BODY()

	//Context에 바인딩된 AI 컨트롤러 (전투 상태 소유자)
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;
};

/**
 * 상태 진입 시 현재 타깃으로 EnemyAIController의 전투 상태를 켜는 Task
 * 전투 진입 시점(거리 등)은 이 태스크를 둔 상태의 진입 조건·트랜지션이 판단하고, 상태 자체는 컨트롤러가 소유
 * 상태를 벗어나도 전투를 해제하지 않음 (해제는 컨트롤러가 타깃 사망·거리로 판단)
 * 진입 후 계속 Running이라 상태를 스스로 완료시키지 않음 (상태 완료는 같은 상태의 다른 태스크나 자식 상태가 결정)
 */
USTRUCT(meta = (DisplayName = "Enter Combat", Category = "AI"))
struct ACTIONPRACTICE_API FEnterCombatTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnterCombatTaskInstanceData;

	FEnterCombatTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
