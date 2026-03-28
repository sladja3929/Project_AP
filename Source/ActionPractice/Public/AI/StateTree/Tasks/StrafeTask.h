#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StrafeTask.generated.h"

class AEnemyAIController;

UENUM()
enum class EStrafePhase : uint8
{
	FirstMove,
	SecondMove,
	Completed
};

USTRUCT()
struct ACTIONPRACTICE_API FStrafeTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context: EnemyAIController
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//Strafe 기준 타깃 (UpdateTargetInfoEvaluator의 DetectedTarget 바인딩)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<AActor> TargetActor = nullptr;

	//횡이동 거리
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float StrafeDistance = 300.0f;

	//현재 Strafe 단계 (내부 관리)
	EStrafePhase Phase = EStrafePhase::FirstMove;

	//현재 이동 방향 (+1 = 오른쪽, -1 = 왼쪽) (내부 관리)
	int32 StrafeDirection = 1;

	//현재 이동 목표 지점 (내부 관리)
	FVector CurrentStrafePoint = FVector::ZeroVector;

#pragma endregion
};

/**
 * 타깃 기준 좌우 횡이동 Task
 * 랜덤 방향 1회 이동 → 반대 방향 1회 이동 → Succeeded
 * 이동 중 SetFocus로 타깃을 바라본다
 */
USTRUCT(meta = (DisplayName = "Strafe", Category = "AI|Movement"))
struct ACTIONPRACTICE_API FStrafeTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FStrafeTaskInstanceData;

	FStrafeTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion

private:
#pragma region "Private Functions"

	//타깃 기준 횡이동 포인트 생성 후 MoveToLocation 요청
	bool RequestStrafeMove(FInstanceDataType& InstanceData) const;

#pragma endregion
};
