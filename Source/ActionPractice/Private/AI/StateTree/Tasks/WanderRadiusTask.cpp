#include "AI/StateTree/Tasks/WanderRadiusTask.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyWanderArea.h"
#include "Characters/EnemyCharacter.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "StateTreeExecutionContext.h"
#include "Navigation/PathFollowingComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWanderRadiusTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWanderRadiusTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

namespace
{
	//배회 범위 (구역 박스 또는 스폰 위치 반경)
	struct FWanderRegion
	{
		const AEnemyWanderArea* Area = nullptr;
		FVector Center = FVector::ZeroVector;
		float Radius = 0.0f;

		bool Contains(const FVector& Location) const
		{
			return Area ? Area->IsInside(Location) : FVector::DistSquared2D(Center, Location) <= FMath::Square(Radius);
		}

		//범위 안 수평 균일 무작위 지점 (내비메시 투영 전)
		FVector RandomPoint() const
		{
			if (Area)
			{
				return Area->GetRandomPointInside();
			}

			const FVector2D Offset = FMath::RandPointInCircle(Radius);
			return Center + FVector(Offset.X, Offset.Y, 0.0f);
		}
	};

	FWanderRegion MakeRegion(const FWanderRadiusTaskInstanceData& InstanceData)
	{
		FWanderRegion Region;
		Region.Area = InstanceData.Area;
		Region.Center = InstanceData.Area ? InstanceData.Area->GetAreaCenter() : InstanceData.Origin;
		Region.Radius = InstanceData.Radius;
		return Region;
	}
}

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

	InstanceData.Area = InstanceData.EnemyCharacter->GetWanderArea();
	InstanceData.Origin = InstanceData.EnemyCharacter->SpawnLocation;

	//무리가 같은 순간에 출발하지 않도록 첫 대기는 0~1배 랜덤
	StartWaiting(InstanceData, FMath::FRand());

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FWanderRadiusTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.bIsWaiting)
	{
		InstanceData.ElapsedWaitTime += DeltaTime;

		if (InstanceData.ElapsedWaitTime < InstanceData.CurrentWaitTime)
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.bIsWaiting = false;

		//지점을 못 찾으면 상태를 끝내지 않고 다음 대기 후 재시도 (다른 구성원 이동으로 자리가 생길 수 있음)
		if (!RequestNewWanderPoint(InstanceData))
		{
			DEBUG_LOG(TEXT("Tick: No wander point found, retry after waiting"));
			StartWaiting(InstanceData, 0.5f);
		}

		return EStateTreeRunStatus::Running;
	}

	UPathFollowingComponent* PathComp = InstanceData.EnemyAIController->GetPathFollowingComponent();

	if (!PathComp)
	{
		return EStateTreeRunStatus::Failed;
	}

	//도착 (또는 이동 중단) → 대기
	if (PathComp->GetStatus() == EPathFollowingStatus::Idle)
	{
		StartWaiting(InstanceData, 1.0f);
		DEBUG_LOG(TEXT("Tick: Arrived at wander point, waiting %.1fs"), InstanceData.CurrentWaitTime);
	}

	return EStateTreeRunStatus::Running;
}

void FWanderRadiusTask::StartWaiting(FInstanceDataType& InstanceData, float WaitScale) const
{
	const float Deviation = FMath::FRandRange(-InstanceData.WaitRandomDeviation, InstanceData.WaitRandomDeviation);

	InstanceData.CurrentWaitTime = FMath::Max(0.0f, InstanceData.WaitTime + Deviation) * WaitScale;
	InstanceData.ElapsedWaitTime = 0.0f;
	InstanceData.bIsWaiting = true;
}

bool FWanderRadiusTask::RequestNewWanderPoint(FInstanceDataType& InstanceData) const
{
	APawn* Pawn = InstanceData.EnemyAIController->GetPawn();
	UNavigationSystemV1* NavSys = Pawn ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld()) : nullptr;

	if (!NavSys)
	{
		DEBUG_LOG(TEXT("RequestNewWanderPoint: Pawn or NavigationSystem is null"));
		return false;
	}

	const FWanderRegion Region = MakeRegion(InstanceData);
	const FVector CurrentLocation = Pawn->GetActorLocation();
	const FVector QueryExtent(50.0f, 50.0f, Pawn->GetSimpleCollisionHalfHeight() + 150.0f);
	const bool bInsideRegion = Region.Contains(CurrentLocation);

	//다른 구성원과 겹치지 않고, 경로가 끊기지 않으며, 우회가 심하지 않은 지점만 허용
	auto IsAcceptable = [&](const FVector& Candidate, float MaxPathLength) -> bool
	{
		if (!Region.Contains(Candidate))
		{
			return false;
		}

		if (InstanceData.Area && InstanceData.Area->IsTooCloseToOthers(Pawn, Candidate))
		{
			return false;
		}

		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(Pawn, CurrentLocation, Candidate, Pawn);

		if (!Path || !Path->IsValid() || Path->IsPartial())
		{
			return false;
		}

		return MaxPathLength <= 0.0f || Path->GetPathLength() <= MaxPathLength;
	};

	FVector ChosenPoint = FVector::ZeroVector;
	bool bFound = false;

	//1. 진행 방향 기준 후보 (구역 안에 있을 때)
	if (bInsideRegion)
	{
		FVector Heading = Pawn->GetActorForwardVector().GetSafeNormal2D();

		if (Heading.IsNearlyZero())
		{
			Heading = FVector::ForwardVector;
		}

		//경로 검사는 서버 게임 스레드 동기 탐색이라 시도 횟수를 작게 유지
		constexpr int32 MaxHeadingAttempts = 8;

		for (int32 Attempt = 0; Attempt < MaxHeadingAttempts && !bFound; ++Attempt)
		{
			//뒤쪽 시도이거나 TurnChance에 걸리면 방향 제한 해제 (구역 경계에서 되돌아오기 포함)
			const bool bWideTurn = Attempt >= MaxHeadingAttempts / 2 || FMath::FRand() < InstanceData.TurnChance;
			const float Spread = bWideTurn ? 180.0f : InstanceData.HeadingSpread;
			const FVector Direction = Heading.RotateAngleAxis(FMath::FRandRange(-Spread, Spread), FVector::UpVector);
			const float StepDistance = FMath::FRandRange(InstanceData.MinStep, FMath::Max(InstanceData.MinStep, InstanceData.MaxStep));

			FNavLocation NavLocation;

			if (!NavSys->ProjectPointToNavigation(CurrentLocation + Direction * StepDistance, NavLocation, QueryExtent))
			{
				continue;
			}

			if (IsAcceptable(NavLocation.Location, StepDistance * InstanceData.MaxPathLengthRatio))
			{
				ChosenPoint = NavLocation.Location;
				bFound = true;
			}
		}
	}

	//2. 구역 밖이거나 방향 후보가 모두 실패하면 범위 안 임의 지점 (복귀 포함, 경로 길이 제한 없음)
	if (!bFound)
	{
		constexpr int32 MaxRandomAttempts = 6;

		for (int32 Attempt = 0; Attempt < MaxRandomAttempts && !bFound; ++Attempt)
		{
			FNavLocation NavLocation;

			if (!NavSys->ProjectPointToNavigation(Region.RandomPoint(), NavLocation, QueryExtent))
			{
				continue;
			}

			if (IsAcceptable(NavLocation.Location, 0.0f))
			{
				ChosenPoint = NavLocation.Location;
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		DEBUG_LOG(TEXT("RequestNewWanderPoint: No valid point (Center=%s, Radius=%.0f, Inside=%d)"), *Region.Center.ToString(), Region.Radius, bInsideRegion);
		return false;
	}

	InstanceData.CurrentTargetPoint = ChosenPoint;

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

	if (InstanceData.Area)
	{
		InstanceData.Area->ReservePoint(Pawn, InstanceData.CurrentTargetPoint);
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
		return FText::FromString(FString::Printf(TEXT("<b>Wander</b> (Step=%.0f~%.0f, Spread=%.0f, Wait=%.1f±%.1fs)"),
			InstanceData->MinStep, InstanceData->MaxStep, InstanceData->HeadingSpread, InstanceData->WaitTime, InstanceData->WaitRandomDeviation));
	}

	return FText::FromString(TEXT("<b>Wander</b>"));
}
#endif //WITH_EDITOR

void FWanderRadiusTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.EnemyAIController)
	{
		InstanceData.EnemyAIController->StopMovement();

		if (InstanceData.Area)
		{
			InstanceData.Area->ReleasePoint(InstanceData.EnemyAIController->GetPawn());
		}

		DEBUG_LOG(TEXT("ExitState: Movement stopped"));
	}

	InstanceData.Area = nullptr;
}
