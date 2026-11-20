#include "AI/StateTree/Tasks/CircleTargetTask.h"
#include "AI/EnemyAIController.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "Navigation/PathFollowingComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogCircleTargetTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogCircleTargetTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FCircleTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController || !InstanceData.EnemyAIController->GetCurrentTargetActor())
	{
		DEBUG_LOG(TEXT("EnterState: No AIController or target"));
		return EStateTreeRunStatus::Failed;
	}

	//남아 있는 포커스가 있으면 몸이 이동 방향 대신 타깃을 향하므로 해제 (시선은 ABP 머리 바라보기 담당)
	InstanceData.EnemyAIController->ClearFocus(EAIFocusPriority::Gameplay);

	InstanceData.Direction = FMath::RandBool() ? 1 : -1;
	InstanceData.ElapsedTime = 0.0f;
	InstanceData.ComputedDuration = FMath::Max(0.1f, InstanceData.Duration + FMath::FRandRange(-InstanceData.DurationDeviation, InstanceData.DurationDeviation));

	if (!RequestArcMove(InstanceData))
	{
		DEBUG_LOG(TEXT("EnterState: No reachable arc point"));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCircleTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController || !InstanceData.EnemyAIController->GetCurrentTargetActor())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime >= InstanceData.ComputedDuration)
	{
		DEBUG_LOG(TEXT("Tick: Duration elapsed (%.1fs) - Task SUCCEEDED"), InstanceData.ComputedDuration);
		return EStateTreeRunStatus::Succeeded;
	}

	const UPathFollowingComponent* PathComp = InstanceData.EnemyAIController->GetPathFollowingComponent();

	if (!PathComp)
	{
		return EStateTreeRunStatus::Failed;
	}

	//현재 호 도착 → 다음 호 (가끔 방향 전환)
	if (PathComp->GetStatus() == EPathFollowingStatus::Idle)
	{
		if (FMath::FRand() < InstanceData.DirectionChangeChance)
		{
			InstanceData.Direction *= -1;
		}

		if (!RequestArcMove(InstanceData))
		{
			DEBUG_LOG(TEXT("Tick: Blocked both ways - Task SUCCEEDED early"));
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Running;
}

bool FCircleTargetTask::RequestArcMove(FInstanceDataType& InstanceData) const
{
	if (TryArcMove(InstanceData, InstanceData.Direction))
	{
		return true;
	}

	//막히면 반대 방향
	InstanceData.Direction *= -1;
	return TryArcMove(InstanceData, InstanceData.Direction);
}

bool FCircleTargetTask::TryArcMove(FInstanceDataType& InstanceData, int32 InDirection) const
{
	AEnemyAIController* AIController = InstanceData.EnemyAIController;
	const APawn* Pawn = AIController->GetPawn();
	const AActor* Target = AIController->GetCurrentTargetActor();
	UNavigationSystemV1* NavSys = Pawn ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld()) : nullptr;

	if (!Pawn || !Target || !NavSys)
	{
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector FromTarget = (PawnLocation - TargetLocation).GetSafeNormal2D();

	if (FromTarget.IsNearlyZero())
	{
		FromTarget = -Pawn->GetActorForwardVector().GetSafeNormal2D();
	}

	//현재 거리를 유지하되 여유를 뺀 범위를 벗어났으면 보정 (멀면 조여 들고 가까우면 벌림)
	const float CurrentRadius = FVector::Dist2D(PawnLocation, TargetLocation);
	const float LowerRadius = InstanceData.MinRadius + InstanceData.EdgeMargin;
	const float UpperRadius = InstanceData.MaxRadius - InstanceData.EdgeMargin;
	const float Radius = LowerRadius <= UpperRadius
		? FMath::Clamp(CurrentRadius, LowerRadius, UpperRadius)
		: (InstanceData.MinRadius + InstanceData.MaxRadius) * 0.5f;
	const float Arc = FMath::Max(5.0f, InstanceData.ArcAngle + FMath::FRandRange(-InstanceData.ArcAngleDeviation, InstanceData.ArcAngleDeviation));
	const FVector Direction = FromTarget.RotateAngleAxis(Arc * InDirection, FVector::UpVector);
	const FVector Candidate = FVector(TargetLocation.X, TargetLocation.Y, PawnLocation.Z) + Direction * Radius;

	FNavLocation NavLocation;
	const FVector QueryExtent(50.0f, 50.0f, Pawn->GetSimpleCollisionHalfHeight() + 150.0f);

	if (!NavSys->ProjectPointToNavigation(Candidate, NavLocation, QueryExtent))
	{
		DEBUG_LOG(TEXT("TryArcMove: Projection failed (Dir=%d)"), InDirection);
		return false;
	}

	const EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
		NavLocation.Location,
		60.0f,  //AcceptanceRadius
		false,  //bStopOnOverlap
		true,   //bUsePathfinding
		false,  //bProjectDestinationToNavigation (이미 보정됨)
		false   //bCanStrafe (몸은 이동 방향을 향함)
	);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		DEBUG_LOG(TEXT("TryArcMove: MoveToLocation failed (Dir=%d)"), InDirection);
		return false;
	}

	DEBUG_LOG(TEXT("TryArcMove: Arc %.0f deg, Radius %.0f (Dir=%d)"), Arc, Radius, InDirection);
	return true;
}

#if WITH_EDITOR
FText FCircleTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Circle Target</b> (Arc=%.0f, R=%.0f~%.0f margin %.0f, %.1f±%.1fs)"),
			InstanceData->ArcAngle, InstanceData->MinRadius, InstanceData->MaxRadius, InstanceData->EdgeMargin, InstanceData->Duration, InstanceData->DurationDeviation));
	}

	return FText::FromString(TEXT("<b>Circle Target</b>"));
}
#endif //WITH_EDITOR

void FCircleTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.EnemyAIController)
	{
		InstanceData.EnemyAIController->StopMovement();
	}
}
