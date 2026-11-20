#include "AI/StateTree/Tasks/CircleTargetTask.h"
#include "AI/EnemyAIController.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"

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

	if (!InstanceData.EnemyAIController || !InstanceData.EnemyAIController->GetPawn() || !InstanceData.EnemyAIController->GetCurrentTargetActor())
	{
		DEBUG_LOG(TEXT("EnterState: No AIController, pawn or target"));
		return EStateTreeRunStatus::Failed;
	}

	//남아 있는 포커스가 있으면 몸이 이동 방향 대신 타깃을 향하므로 해제 (시선은 ABP 머리 바라보기 담당)
	//이전 상태의 경로 이동이 남아 있으면 이동 입력과 섞이므로 정지
	InstanceData.EnemyAIController->ClearFocus(EAIFocusPriority::Gameplay);
	InstanceData.EnemyAIController->StopMovement();

	const APawn* Pawn = InstanceData.EnemyAIController->GetPawn();

	InstanceData.Direction = FMath::RandBool() ? 1 : -1;
	InstanceData.ElapsedTime = 0.0f;
	InstanceData.ComputedDuration = FMath::Max(0.1f, InstanceData.Duration + FMath::FRandRange(-InstanceData.DurationDeviation, InstanceData.DurationDeviation));
	InstanceData.PreviousBearing = GetBearing(InstanceData);
	InstanceData.ObstacleCheckElapsed = 0.0f;

	//현재 몸 방향에서 시작해 조향 목표 쪽으로 서서히 돌아가도록 (진입 순간 몸이 꺾이지 않음)
	InstanceData.MoveDirection = Pawn->GetActorForwardVector().GetSafeNormal2D();

	if (InstanceData.MoveDirection.IsNearlyZero())
	{
		InstanceData.MoveDirection = FVector::ForwardVector;
	}

	StartSegment(InstanceData);

	if (!CheckObstacle(InstanceData))
	{
		DEBUG_LOG(TEXT("EnterState: Blocked both ways"));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCircleTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	APawn* Pawn = InstanceData.EnemyAIController ? InstanceData.EnemyAIController->GetPawn() : nullptr;

	if (!Pawn || !InstanceData.EnemyAIController->GetCurrentTargetActor())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime >= InstanceData.ComputedDuration)
	{
		DEBUG_LOG(TEXT("Tick: Duration elapsed (%.1fs) - Task SUCCEEDED"), InstanceData.ComputedDuration);
		return EStateTreeRunStatus::Succeeded;
	}

	//타깃 기준 방위각 변화로 선회 진행량 누적 (타깃이 움직여도 실제로 돈 각도 기준)
	const float Bearing = GetBearing(InstanceData);
	InstanceData.SegmentRemainingAngle -= FMath::FindDeltaAngleDegrees(InstanceData.PreviousBearing, Bearing) * InstanceData.Direction;
	InstanceData.PreviousBearing = Bearing;

	bool bCheckNow = false;

	//구간을 다 돌면 가끔 방향 전환
	if (InstanceData.SegmentRemainingAngle <= 0.0f)
	{
		if (FMath::FRand() < InstanceData.DirectionChangeChance)
		{
			InstanceData.Direction *= -1;
			bCheckNow = true;
		}

		StartSegment(InstanceData);
	}

	InstanceData.ObstacleCheckElapsed += DeltaTime;

	if (bCheckNow || InstanceData.ObstacleCheckElapsed >= InstanceData.ObstacleCheckInterval)
	{
		if (!CheckObstacle(InstanceData))
		{
			DEBUG_LOG(TEXT("Tick: Blocked both ways - Task SUCCEEDED early"));
			return EStateTreeRunStatus::Succeeded;
		}
	}

	//이동 입력 방향을 조향 목표 쪽으로 TurnRate만큼만 돌림 (몸 회전은 이 방향을 따라감)
	//벡터 회전 보간은 정반대 방향일 때 수평축으로 돌아 방향이 바뀌지 않으므로 yaw만 보간
	const FVector ToSteerPoint = (ComputeSteerPoint(InstanceData, InstanceData.Direction) - Pawn->GetActorLocation()).GetSafeNormal2D();

	if (!ToSteerPoint.IsNearlyZero())
	{
		const float CurrentYaw = InstanceData.MoveDirection.Rotation().Yaw;
		const float MaxStep = InstanceData.TurnRate * DeltaTime;
		const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw, ToSteerPoint.Rotation().Yaw), -MaxStep, MaxStep);
		InstanceData.MoveDirection = FRotator(0.0f, CurrentYaw + Step, 0.0f).Vector();
	}

	Pawn->AddMovementInput(InstanceData.MoveDirection, 1.0f);
	return EStateTreeRunStatus::Running;
}

bool FCircleTargetTask::CheckObstacle(FInstanceDataType& InstanceData) const
{
	InstanceData.ObstacleCheckElapsed = 0.0f;

	if (IsPathClear(InstanceData, InstanceData.Direction))
	{
		return true;
	}

	//막히면 반대 방향으로 새 구간
	InstanceData.Direction *= -1;
	StartSegment(InstanceData);
	return IsPathClear(InstanceData, InstanceData.Direction);
}

bool FCircleTargetTask::IsPathClear(const FInstanceDataType& InstanceData, int32 InDirection) const
{
	AEnemyAIController* AIController = InstanceData.EnemyAIController;
	const APawn* Pawn = AIController->GetPawn();
	UNavigationSystemV1* NavSys = Pawn ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld()) : nullptr;

	if (!Pawn || !NavSys)
	{
		return false;
	}

	FNavLocation NavLocation;
	const FVector QueryExtent(50.0f, 50.0f, Pawn->GetSimpleCollisionHalfHeight() + 150.0f);

	if (!NavSys->ProjectPointToNavigation(ComputeSteerPoint(InstanceData, InDirection), NavLocation, QueryExtent))
	{
		DEBUG_LOG(TEXT("IsPathClear: Steer point off navmesh (Dir=%d)"), InDirection);
		return false;
	}

	//몸이 내비메시 경계 밖으로 살짝 밀린 프레임은 시작점이 없어 레이캐스트가 막힘으로 나오므로 이번 검사를 건너뜀
	FNavLocation StartNavLocation;

	if (!NavSys->ProjectPointToNavigation(Pawn->GetNavAgentLocation(), StartNavLocation, QueryExtent))
	{
		DEBUG_LOG(TEXT("IsPathClear: Pawn off navmesh, skip check (Dir=%d)"), InDirection);
		return true;
	}

	//내비메시 위 직선 이동이 끊기면(벽·절벽 등 내비 경계) 막힌 것으로 판정
	FVector HitLocation;

	if (UNavigationSystemV1::NavigationRaycast(AIController, StartNavLocation.Location, NavLocation.Location, HitLocation, nullptr, AIController))
	{
		DEBUG_LOG(TEXT("IsPathClear: Navigation raycast blocked (Dir=%d)"), InDirection);
		return false;
	}

	return true;
}

void FCircleTargetTask::StartSegment(FInstanceDataType& InstanceData) const
{
	InstanceData.SegmentRemainingAngle = FMath::Max(5.0f, InstanceData.ArcAngle + FMath::FRandRange(-InstanceData.ArcAngleDeviation, InstanceData.ArcAngleDeviation));
	DEBUG_LOG(TEXT("StartSegment: Arc %.0f deg (Dir=%d)"), InstanceData.SegmentRemainingAngle, InstanceData.Direction);
}

float FCircleTargetTask::GetBearing(const FInstanceDataType& InstanceData)
{
	const APawn* Pawn = InstanceData.EnemyAIController ? InstanceData.EnemyAIController->GetPawn() : nullptr;
	const AActor* Target = InstanceData.EnemyAIController ? InstanceData.EnemyAIController->GetCurrentTargetActor() : nullptr;

	if (!Pawn || !Target)
	{
		return 0.0f;
	}

	const FVector FromTarget = Pawn->GetActorLocation() - Target->GetActorLocation();
	return FMath::RadiansToDegrees(FMath::Atan2(FromTarget.Y, FromTarget.X));
}

FVector FCircleTargetTask::ComputeSteerPoint(const FInstanceDataType& InstanceData, int32 InDirection)
{
	const APawn* Pawn = InstanceData.EnemyAIController ? InstanceData.EnemyAIController->GetPawn() : nullptr;
	const AActor* Target = InstanceData.EnemyAIController ? InstanceData.EnemyAIController->GetCurrentTargetActor() : nullptr;

	if (!Pawn || !Target)
	{
		return FVector::ZeroVector;
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
	const float KeepRadius = LowerRadius <= UpperRadius
		? FMath::Clamp(CurrentRadius, LowerRadius, UpperRadius)
		: (InstanceData.MinRadius + InstanceData.MaxRadius) * 0.5f;

	//같은 반경의 앞 지점을 향하면 진행 방향이 접선보다 안쪽(현 방향)이라 반경이 계속 줄어듦
	//반경을 1/cos(LookAhead)만큼 키운 지점을 향하면 진행 방향이 KeepRadius 원의 접선과 일치
	const float SteerRadius = KeepRadius / FMath::Cos(FMath::DegreesToRadians(InstanceData.LookAheadAngle));
	const FVector Direction = FromTarget.RotateAngleAxis(InstanceData.LookAheadAngle * InDirection, FVector::UpVector);

	return FVector(TargetLocation.X, TargetLocation.Y, PawnLocation.Z) + Direction * SteerRadius;
}

#if WITH_EDITOR
FText FCircleTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Circle Target</b> (Arc=%.0f, LookAhead=%.0f, Turn=%.0f/s, R=%.0f~%.0f margin %.0f, %.1f±%.1fs)"),
			InstanceData->ArcAngle, InstanceData->LookAheadAngle, InstanceData->TurnRate, InstanceData->MinRadius, InstanceData->MaxRadius, InstanceData->EdgeMargin, InstanceData->Duration, InstanceData->DurationDeviation));
	}

	return FText::FromString(TEXT("<b>Circle Target</b>"));
}
#endif //WITH_EDITOR

void FCircleTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	//이동 입력은 매 틱 주는 방식이라 입력이 끊기면 CMC 감속으로 멈춤. 혹시 남은 경로 이동만 정리
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.EnemyAIController)
	{
		InstanceData.EnemyAIController->StopMovement();
	}
}
