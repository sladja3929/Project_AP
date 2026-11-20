#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "CircleTargetTask.generated.h"

class AEnemyAIController;

USTRUCT()
struct FCircleTargetTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context에 바인딩된 AI 컨트롤러 (타깃 조회, 이동 요청)
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//한 번 이동할 때 타깃 둘레를 도는 각도와 랜덤 편차
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float ArcAngle = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float ArcAngleDeviation = 10.0f;

	//유지할 반경 범위 (현재 거리를 이 범위로 보정)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float MinRadius = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float MaxRadius = 700.0f;

	//반경 범위 양 끝에서 안쪽으로 둘 여유 (도착 오차·타깃 이동으로 범위 경계를 넘어 완료 후 다른 거리 상태가 선택되는 것 방지)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float EdgeMargin = 80.0f;

	//선회 지속 시간 (초)과 랜덤 편차
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1"))
	float Duration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float DurationDeviation = 1.0f;

	//다음 호로 넘어갈 때 방향을 바꿀 확률
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DirectionChangeChance = 0.15f;

	//+1 시계 반대 / -1 시계 방향 (타깃 기준)
	int32 Direction = 1;

	float ElapsedTime = 0.0f;
	float ComputedDuration = 0.0f;

#pragma endregion
};

/**
 * 타깃 둘레를 호를 따라 걸으며 견제하는 Task (옆걸음 모션이 없는 사족 보행용)
 * - 몸은 이동 방향을 향하고(bOrientRotationToMovement 유지), 타깃 쪽 시선은 ABP 머리 바라보기가 담당
 * - 타깃→자신 방향을 ArcAngle만큼 돌린 지점(반경은 MinRadius~MaxRadius 안쪽 EdgeMargin 범위로 보정)으로 이동을 반복
 * - 막히면 방향을 바꿔 재시도, 그래도 막히면 조기 완료. Duration이 지나면 Succeeded
 * - 이동 속도는 같은 상태의 GE 태스크가 지정
 */
USTRUCT(meta = (DisplayName = "Circle Target", Category = "AI|Movement"))
struct ACTIONPRACTICE_API FCircleTargetTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FCircleTargetTaskInstanceData;

	FCircleTargetTask() = default;

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

	//현재 방향으로 다음 호 지점 이동 요청 (실패 시 반대 방향 한 번 재시도)
	bool RequestArcMove(FInstanceDataType& InstanceData) const;

	//지정 방향 호 지점 계산·이동 요청
	bool TryArcMove(FInstanceDataType& InstanceData, int32 InDirection) const;

#pragma endregion
};
