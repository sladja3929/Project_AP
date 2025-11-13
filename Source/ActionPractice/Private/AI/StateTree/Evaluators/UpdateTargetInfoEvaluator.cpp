#include "AI/StateTree/Evaluators/UpdateTargetInfoEvaluator.h"
#include "AI/EnemyAIController.h"
#include "Characters/ActionPracticeCharacter.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogUpdateTargetInfoEvaluator, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogUpdateTargetInfoEvaluator, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

void FUpdateTargetInfoEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.SourceActor)
	{
		DEBUG_LOG(TEXT("SourceActor is not valid in UpdateTargetInfoEvaluator"));
		return;
	}

	if (!InstanceData.AIController)
	{
		DEBUG_LOG(TEXT("AIController is not valid in UpdateTargetInfoEvaluator"));
		return;
	}

	DEBUG_LOG(TEXT("UpdateTargetInfoEvaluator TreeStart"));
}

void FUpdateTargetInfoEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.DetectedTarget = nullptr;
	InstanceData.DistanceToTarget = -1.0f;
	InstanceData.AngleToTarget = 0.0f;
	InstanceData.bTargetDetected = false;

	DEBUG_LOG(TEXT("UpdateTargetInfoEvaluator TreeStop"));
}

void FUpdateTargetInfoEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UpdateTargetInfo(Context);
}

void FUpdateTargetInfoEvaluator::UpdateTargetInfo(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AIController || !InstanceData.AIController->CurrentTarget.IsValid() || !InstanceData.SourceActor)
	{
		InstanceData.DetectedTarget = nullptr;
		InstanceData.DistanceToTarget = -1.0f;
		InstanceData.AngleToTarget = 0.0f;
		InstanceData.bTargetDetected = false;
		return;
	}

	AActor* TargetActor = InstanceData.AIController->GetCurrentTargetActor();
	if (!TargetActor)
	{
		InstanceData.DetectedTarget = nullptr;
		InstanceData.DistanceToTarget = -1.0f;
		InstanceData.AngleToTarget = 0.0f;
		InstanceData.bTargetDetected = false;
		return;
	}

	InstanceData.DetectedTarget = TargetActor;

	const FVector SourceLocation = InstanceData.SourceActor->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	//거리 계산
	const float Distance = FVector::Dist(SourceLocation, TargetLocation);
	InstanceData.DistanceToTarget = Distance;

	//각도 계산 (Enemy의 정면 기준 Target과의 Yaw 각도)
	const FVector DirectionToTarget = (TargetLocation - SourceLocation).GetSafeNormal();
	const FVector ForwardVector = InstanceData.SourceActor->GetActorForwardVector();

	//내적을 이용한 각도 계산 (0~180도)
	const float DotProduct = FVector::DotProduct(ForwardVector, DirectionToTarget);
	const float AngleRadians = FMath::Acos(DotProduct);
	const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

	//외적을 이용하여 좌우 방향 판별 (음수면 왼쪽, 양수면 오른쪽)
	const FVector CrossProduct = FVector::CrossProduct(ForwardVector, DirectionToTarget);
	const float SignedAngle = (CrossProduct.Z < 0.0f) ? -AngleDegrees : AngleDegrees;

	InstanceData.AngleToTarget = SignedAngle;
	InstanceData.bTargetDetected = true;

	//AIController의 CurrentTarget 구조체 갱신
	InstanceData.AIController->CurrentTarget.Distance = Distance;
	InstanceData.AIController->CurrentTarget.AngleToTarget = SignedAngle;

	DEBUG_LOG(TEXT("Target Info Updated - Distance: %.2f, Angle: %.2f"), Distance, SignedAngle);
}
