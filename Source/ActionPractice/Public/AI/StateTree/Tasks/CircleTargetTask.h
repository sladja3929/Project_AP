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

	//Context에 바인딩된 AI 컨트롤러 (타깃·폰 조회, 내비 질의)
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//방향 전환 판정 전까지 타깃 둘레를 도는 각도와 랜덤 편차
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "5.0", ClampMax = "120.0"))
	float ArcAngle = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float ArcAngleDeviation = 10.0f;

	//조향 목표를 현재 위치에서 원 둘레로 얼마나 앞에 둘지 (작을수록 원에 붙어 돌고 몸이 접선 방향에 가까움)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float LookAheadAngle = 20.0f;

	//진행 방향이 조향 목표 쪽으로 돌아가는 최대 속도 (도/초). 작을수록 몸 회전이 부드럽고 방향 전환 시 크게 돎
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "10.0"))
	float TurnRate = 90.0f;

	//진행 방향 장애물(내비 경계) 검사 주기 (초)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.05"))
	float ObstacleCheckInterval = 0.25f;

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

	//현재 구간에서 남은 선회 각도 (0 이하가 되면 방향 전환 판정 후 새 구간)
	float SegmentRemainingAngle = 0.0f;

	//직전 틱의 타깃 기준 방위각 (선회 진행량 누적용)
	float PreviousBearing = 0.0f;

	float ObstacleCheckElapsed = 0.0f;

	//현재 이동 입력 방향 (수평 단위 벡터, TurnRate로 조향 목표를 향해 보간)
	FVector MoveDirection = FVector::ForwardVector;

#pragma endregion
};

/**
 * 타깃 둘레를 호를 따라 걸으며 견제하는 Task (옆걸음 모션이 없는 사족 보행용)
 * - 몸은 이동 방향을 향하고(bOrientRotationToMovement 유지), 타깃 쪽 시선은 ABP 머리 바라보기가 담당
 * - 경로 이동(MoveTo) 대신 매 틱 이동 입력으로 조향 (steering behavior)
 *   - 조향 목표: 현재 타깃 위치 기준 LookAheadAngle만큼 앞선 원 둘레 지점 (반경은 MinRadius~MaxRadius 안쪽 EdgeMargin 범위로 보정)
 *   - 이동 입력 방향을 조향 목표 쪽으로 TurnRate(도/초)씩만 돌려, 타깃이 움직여도 몸 측면이 타깃을 향한 채 부드럽게 따라 돎
 *   - 경로 재요청마다 가속 방향이 꺾여 몸이 툭툭 도는 문제가 없음
 * - ArcAngle만큼 돌 때마다 DirectionChangeChance로 방향 전환
 * - ObstacleCheckInterval마다 조향 목표까지 내비 레이캐스트. 막히면 반대 방향, 양쪽 다 막히면 조기 완료. Duration이 지나면 Succeeded
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

	//현재 방향 진행로 검사 (막히면 반대 방향으로 바꾸고 새 구간, 양쪽 다 막히면 false)
	bool CheckObstacle(FInstanceDataType& InstanceData) const;

	//지정 방향 조향 목표까지 내비메시 위로 이동 가능한지
	bool IsPathClear(const FInstanceDataType& InstanceData, int32 InDirection) const;

	//지정 방향 조향 목표 (원 둘레 LookAheadAngle 앞, 보정 반경)
	static FVector ComputeSteerPoint(const FInstanceDataType& InstanceData, int32 InDirection);

	//새 선회 구간 시작 (남은 각도 초기화)
	void StartSegment(FInstanceDataType& InstanceData) const;

	//타깃 기준 자신의 수평 방위각 (도)
	static float GetBearing(const FInstanceDataType& InstanceData);

#pragma endregion
};
