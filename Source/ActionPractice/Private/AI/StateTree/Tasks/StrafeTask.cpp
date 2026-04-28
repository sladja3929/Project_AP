#include "AI/StateTree/Tasks/StrafeTask.h"
#include "AI/EnemyAIController.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogStrafeTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogStrafeTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FStrafeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController)
	{
		DEBUG_LOG(TEXT("EnterState: EnemyAIController is null"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.TargetActor)
	{
		DEBUG_LOG(TEXT("EnterState: TargetActor is null"));
		return EStateTreeRunStatus::Failed;
	}

	//Strafe 중 타깃을 바라보도록 CMC 설정 전환
	APawn* OwnerPawn = InstanceData.EnemyAIController->GetPawn();
	if (UCharacterMovementComponent* MoveComp = OwnerPawn ? Cast<ACharacter>(OwnerPawn)->GetCharacterMovement() : nullptr)
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->bUseControllerDesiredRotation = true;
	}
	InstanceData.EnemyAIController->SetFocus(InstanceData.TargetActor.Get());

	//랜덤 초기 방향 결정
	InstanceData.StrafeDirection = FMath::RandBool() ? 1 : -1;
	InstanceData.Phase = EStrafePhase::FirstMove;

	if (!RequestStrafeMove(InstanceData))
	{
		DEBUG_LOG(TEXT("EnterState: Failed to request first strafe move"));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStrafeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.Phase == EStrafePhase::Completed)
	{
		return EStateTreeRunStatus::Succeeded;
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
		if (InstanceData.Phase == EStrafePhase::FirstMove)
		{
			//첫 번째 이동 완료 → 반대 방향으로 두 번째 이동
			InstanceData.StrafeDirection *= -1;
			InstanceData.Phase = EStrafePhase::SecondMove;

			if (!RequestStrafeMove(InstanceData))
			{
				DEBUG_LOG(TEXT("Tick: Failed to request second strafe move"));
				return EStateTreeRunStatus::Failed;
			}
		}
		else if (InstanceData.Phase == EStrafePhase::SecondMove)
		{
			//두 번째 이동 완료 → Succeeded
			InstanceData.Phase = EStrafePhase::Completed;
			DEBUG_LOG(TEXT("Tick: Strafe completed"));
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FStrafeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.EnemyAIController)
	{
		InstanceData.EnemyAIController->StopMovement();
		InstanceData.EnemyAIController->ClearFocus(EAIFocusPriority::Gameplay);

		//CMC 설정 복원
		APawn* OwnerPawn = InstanceData.EnemyAIController->GetPawn();
		if (UCharacterMovementComponent* MoveComp = OwnerPawn ? Cast<ACharacter>(OwnerPawn)->GetCharacterMovement() : nullptr)
		{
			MoveComp->bOrientRotationToMovement = true;
			MoveComp->bUseControllerDesiredRotation = false;
		}

		DEBUG_LOG(TEXT("ExitState: Movement stopped, Focus cleared, CMC restored"));
	}
}

bool FStrafeTask::RequestStrafeMove(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.TargetActor)
	{
		DEBUG_LOG(TEXT("RequestStrafeMove: TargetActor is null"));
		return false;
	}

	APawn* OwnerPawn = InstanceData.EnemyAIController->GetPawn();
	if (!OwnerPawn)
	{
		DEBUG_LOG(TEXT("RequestStrafeMove: OwnerPawn is null"));
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerPawn->GetWorld());
	if (!NavSys)
	{
		DEBUG_LOG(TEXT("RequestStrafeMove: NavigationSystem is null"));
		return false;
	}

	const FVector AILocation = OwnerPawn->GetActorLocation();
	const FVector TargetLocation = InstanceData.TargetActor->GetActorLocation();

	//AI → Target 방향의 수직 벡터 (좌우)
	const FVector DirToTarget = (TargetLocation - AILocation).GetSafeNormal2D();
	const FVector RightDir = FVector::CrossProduct(FVector::UpVector, DirToTarget);

	//횡이동 포인트 계산
	const FVector StrafePoint = AILocation + (RightDir * InstanceData.StrafeDistance * InstanceData.StrafeDirection);

	//NavMesh 위로 보정
	FNavLocation NavLocation;
	const bool bProjected = NavSys->ProjectPointToNavigation(StrafePoint, NavLocation);

	if (!bProjected)
	{
		DEBUG_LOG(TEXT("RequestStrafeMove: ProjectPointToNavigation failed (Point=%s)"), *StrafePoint.ToString());
		return false;
	}

	InstanceData.CurrentStrafePoint = NavLocation.Location;

	const EPathFollowingRequestResult::Type Result = InstanceData.EnemyAIController->MoveToLocation(
		InstanceData.CurrentStrafePoint,
		50.0f, //AcceptanceRadius
		true,  //bStopOnOverlap
		true,  //bUsePathfinding
		false, //bProjectDestinationToNavigation (이미 보정됨)
		true   //bCanStrafe
	);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		DEBUG_LOG(TEXT("RequestStrafeMove: MoveToLocation failed (Target=%s)"), *InstanceData.CurrentStrafePoint.ToString());
		return false;
	}

	DEBUG_LOG(TEXT("RequestStrafeMove: Moving to %s (Dir=%d, Phase=%d)"),
		*InstanceData.CurrentStrafePoint.ToString(),
		InstanceData.StrafeDirection,
		static_cast<int32>(InstanceData.Phase));
	return true;
}

#if WITH_EDITOR
FText FStrafeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Strafe</b> (Dist=%.0f)"),
			InstanceData->StrafeDistance));
	}

	return FText::FromString(TEXT("<b>Strafe</b>"));
}
#endif //WITH_EDITOR
