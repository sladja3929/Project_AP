#include "AI/StateTree/Conditions/RetreatPathCondition.h"
#include "AI/EnemyAIController.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogRetreatPathCondition, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogRetreatPathCondition, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool FRetreatPathCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyAIController* AIController = InstanceData.EnemyAIController;
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;

	if (!Pawn)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());

	if (!NavSys)
	{
		DEBUG_LOG(TEXT("TestCondition: No navigation system"));
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();

	//타깃 반대 방향 (수평). 타깃이 없거나 겹쳐 있으면 정면 반대
	FVector RetreatDirection = -Pawn->GetActorForwardVector().GetSafeNormal2D();

	if (const AActor* Target = AIController->GetCurrentTargetActor())
	{
		const FVector AwayFromTarget = (PawnLocation - Target->GetActorLocation()).GetSafeNormal2D();

		if (!AwayFromTarget.IsNearlyZero())
		{
			RetreatDirection = AwayFromTarget;
		}
	}

	//캡슐 중심에서 바닥의 내비메시까지 내려 찾음
	const FVector QueryExtent(50.0f, 50.0f, Pawn->GetSimpleCollisionHalfHeight() + 100.0f);
	FNavLocation StartLocation;

	if (!NavSys->ProjectPointToNavigation(PawnLocation, StartLocation, QueryExtent))
	{
		DEBUG_LOG(TEXT("TestCondition: Pawn is not on navmesh"));
		return false;
	}

	const FVector EndLocation = StartLocation.Location + RetreatDirection * InstanceData.RetreatDistance;
	FVector HitLocation = EndLocation;

	//true면 경로가 메시 경계·장애물에 막힘. 높이는 보지 않음 (메시로 이어진 경사·계단은 걸어서 이동 가능하므로 통과)
	const bool bBlocked = UNavigationSystemV1::NavigationRaycast(Pawn, StartLocation.Location, EndLocation, HitLocation, nullptr, AIController);

	DEBUG_LOG(TEXT("TestCondition: Blocked=%d (Hit %.0f / %.0f)"), bBlocked, FVector::Dist2D(StartLocation.Location, HitLocation), InstanceData.RetreatDistance);

	return !bBlocked;
}

#if WITH_EDITOR
FText FRetreatPathCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Retreat Path Clear</b> %.0f"), InstanceData->RetreatDistance));
	}

	return FText::FromString(TEXT("<b>Retreat Path Clear</b>"));
}
#endif //WITH_EDITOR
