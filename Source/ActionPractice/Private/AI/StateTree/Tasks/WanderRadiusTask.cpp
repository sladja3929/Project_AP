#include "AI/StateTree/Tasks/WanderRadiusTask.h"
#include "AI/EnemyAIController.h"
#include "Characters/EnemyCharacter.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "Navigation/PathFollowingComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWanderRadiusTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWanderRadiusTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FWanderRadiusTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController)
	{
		DEBUG_LOG(TEXT("EnterState: EnemyAIController is null"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.EnemyCharacter)
	{
		DEBUG_LOG(TEXT("EnterState: EnemyCharacter is null"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Origin = InstanceData.EnemyCharacter->SpawnLocation;
	InstanceData.bIsWaiting = false;
	InstanceData.ElapsedWaitTime = 0.0f;

	if (!RequestNewWanderPoint(InstanceData))
	{
		DEBUG_LOG(TEXT("EnterState: Failed to find initial wander point"));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FWanderRadiusTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	//대기 상태 처리
	if (InstanceData.bIsWaiting)
	{
		InstanceData.ElapsedWaitTime += DeltaTime;
		if (InstanceData.ElapsedWaitTime >= InstanceData.WaitTime)
		{
			InstanceData.bIsWaiting = false;
			InstanceData.ElapsedWaitTime = 0.0f;

			if (!RequestNewWanderPoint(InstanceData))
			{
				DEBUG_LOG(TEXT("Tick: Failed to find next wander point"));
				return EStateTreeRunStatus::Failed;
			}
		}

		return EStateTreeRunStatus::Running;
	}

	//이동 완료 확인
	UPathFollowingComponent* PathComp = InstanceData.EnemyAIController->GetPathFollowingComponent();
	if (!PathComp)
	{
		return EStateTreeRunStatus::Failed;
	}

	const EPathFollowingStatus::Type MoveStatus = PathComp->GetStatus();

	if (MoveStatus == EPathFollowingStatus::Idle)
	{
		//이동 완료 → 대기 시작
		InstanceData.bIsWaiting = true;
		InstanceData.ElapsedWaitTime = 0.0f;
		DEBUG_LOG(TEXT("Tick: Arrived at wander point, waiting %.1fs"), InstanceData.WaitTime);
	}

	return EStateTreeRunStatus::Running;
}

void FWanderRadiusTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.EnemyAIController)
	{
		InstanceData.EnemyAIController->StopMovement();
		DEBUG_LOG(TEXT("ExitState: Movement stopped"));
	}
}

bool FWanderRadiusTask::RequestNewWanderPoint(FInstanceDataType& InstanceData) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(InstanceData.EnemyAIController->GetWorld());
	if (!NavSys)
	{
		DEBUG_LOG(TEXT("RequestNewWanderPoint: NavigationSystem is null"));
		return false;
	}

	const FVector CurrentLocation = InstanceData.EnemyAIController->GetPawn()->GetActorLocation();
	const float MinDistSq = InstanceData.MinDistance * InstanceData.MinDistance;

	constexpr int32 MaxAttempts = 10;
	FNavLocation NavLocation;
	bool bFound = false;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		if (!NavSys->GetRandomReachablePointInRadius(InstanceData.Origin, InstanceData.Radius, NavLocation))
		{
			continue;
		}

		if (FVector::DistSquared(CurrentLocation, NavLocation.Location) >= MinDistSq)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		DEBUG_LOG(TEXT("RequestNewWanderPoint: No valid point found after %d attempts (Origin=%s, Radius=%.1f, MinDist=%.1f)"),
			MaxAttempts, *InstanceData.Origin.ToString(), InstanceData.Radius, InstanceData.MinDistance);
		return false;
	}

	InstanceData.CurrentTargetPoint = NavLocation.Location;

	const EPathFollowingRequestResult::Type Result = InstanceData.EnemyAIController->MoveToLocation(
		InstanceData.CurrentTargetPoint,
		50.0f, //AcceptanceRadius
		true,  //bStopOnOverlap
		true,  //bUsePathfinding
		false, //bProjectDestinationToNavigation (이미 NavMesh 위)
		true   //bCanStrafe
	);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		DEBUG_LOG(TEXT("RequestNewWanderPoint: MoveToLocation failed (Target=%s)"), *InstanceData.CurrentTargetPoint.ToString());
		return false;
	}

	DEBUG_LOG(TEXT("RequestNewWanderPoint: Moving to %s"), *InstanceData.CurrentTargetPoint.ToString());
	return true;
}

#if WITH_EDITOR
FText FWanderRadiusTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Wander Radius</b> (R=%.0f, Min=%.0f, Wait=%.1fs)"),
			InstanceData->Radius, InstanceData->MinDistance, InstanceData->WaitTime));
	}

	return FText::FromString(TEXT("<b>Wander Radius</b>"));
}
#endif //WITH_EDITOR
