#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "WanderRadiusTask.generated.h"

class AEnemyAIController;
class AEnemyCharacter;

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

	//배회 중심 위치 (EnterState에서 EnemyCharacter::SpawnLocation으로 초기화)
	FVector Origin = FVector::ZeroVector;

	//배회 반경
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Radius = 600.0f;

	//최소 이동 거리 (현재 위치에서 이 값 미만인 포인트는 무시)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MinDistance = 200.0f;

	//포인트 도착 후 대기 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WaitTime = 2.0f;

	//현재 이동 목표 지점 (내부 관리)
	FVector CurrentTargetPoint = FVector::ZeroVector;

	//대기 중 경과 시간 (내부 관리)
	float ElapsedWaitTime = 0.0f;

	//현재 대기 중인지 여부 (내부 관리)
	bool bIsWaiting = false;

#pragma endregion
};

/**
 * 스폰 위치 기준 반경 내 NavMesh 랜덤 포인트로 배회하는 Task
 * 도착 시 WaitTime만큼 대기 후 새 포인트를 생성하여 반복
 */
USTRUCT(meta = (DisplayName = "Wander Radius", Category = "AI|Movement"))
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

	//NavMesh 위 랜덤 포인트 생성 후 MoveToLocation 요청
	bool RequestNewWanderPoint(FInstanceDataType& InstanceData) const;

#pragma endregion
};
