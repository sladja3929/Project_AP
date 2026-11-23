#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "WanderRadiusTask.generated.h"

class AEnemyAIController;
class AEnemyCharacter;
class AEnemyWanderArea;

USTRUCT()
struct ACTIONPRACTICE_API FWanderRadiusTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context: EnemyAIController
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//Context: EnemyCharacter
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyCharacter> EnemyCharacter = nullptr;

	//배회 구역이 없을 때 스폰 위치 기준 배회 반경
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float Radius = 600.0f;

	//한 번에 이동할 최소·최대 수평 거리
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float MinStep = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float MaxStep = 800.0f;

	//다음 지점 방향의 최대 꺾임 각 (현재 진행 방향 기준, 작을수록 직선적으로 어슬렁거림)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HeadingSpread = 60.0f;

	//방향 제한 없이 크게 도는 확률
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TurnChance = 0.2f;

	//직선 거리 대비 허용 경로 길이 배율 (넘으면 우회가 심한 지점으로 보고 제외)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1.0"))
	float MaxPathLengthRatio = 1.6f;

	//지점 도착 후 대기 시간 (초)과 랜덤 편차
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float WaitTime = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float WaitRandomDeviation = 1.0f;

	//이번 실행에서 사용하는 배회 구역 (EnterState에서 EnemyCharacter::WanderArea로 초기화)
	UPROPERTY()
	TObjectPtr<AEnemyWanderArea> Area = nullptr;

	//구역이 없을 때 배회 중심 (스폰 위치)
	FVector Origin = FVector::ZeroVector;

	//현재 이동 목표 지점
	FVector CurrentTargetPoint = FVector::ZeroVector;

	//이번 대기 시간과 경과 시간
	float CurrentWaitTime = 0.0f;
	float ElapsedWaitTime = 0.0f;

	//현재 대기 중인지 여부
	bool bIsWaiting = false;

#pragma endregion
};

/**
 * Idle 배회 Task
 * - EnemyCharacter::WanderArea가 있으면 그 구역 안, 없으면 스폰 위치 반경 안에서 배회
 * - 다음 지점은 현재 진행 방향 기준 HeadingSpread 안, MinStep~MaxStep 거리에서 골라 지그재그 대신 어슬렁거리는 궤적을 만듦 (TurnChance로 가끔 크게 방향 전환)
 * - 같은 구역의 다른 구성원 목적지와 가까운 지점, 경로가 끊기거나 우회가 심한 지점은 제외
 * - 시작 시 랜덤 대기로 무리의 출발 시점을 분산하고, 도착 후 WaitTime ± WaitRandomDeviation 대기
 * - 구역 밖(추적 후 복귀 등)이면 구역 안 지점으로 먼저 돌아감
 */
USTRUCT(meta = (DisplayName = "Wander", Category = "AI|Movement"))
struct ACTIONPRACTICE_API FWanderRadiusTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FWanderRadiusTaskInstanceData;

	FWanderRadiusTask() = default;

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

	//다음 배회 지점 탐색 후 MoveToLocation 요청
	bool RequestNewWanderPoint(FInstanceDataType& InstanceData) const;

	//다음 대기 시작 (WaitTime ± WaitRandomDeviation)
	void StartWaiting(FInstanceDataType& InstanceData, float WaitScale) const;

#pragma endregion
};
