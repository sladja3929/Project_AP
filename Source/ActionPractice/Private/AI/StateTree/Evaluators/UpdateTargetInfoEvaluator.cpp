#include "AI/StateTree/Evaluators/UpdateTargetInfoEvaluator.h"
#include "AI/EnemyAIController.h"
#include "Characters/ActionPracticeCharacter.h"
#include "StateTreeExecutionContext.h"
#include "Characters/EnemyCharacter.h"

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

	if (!InstanceData.EnemyCharacter)
	{
		DEBUG_LOG(TEXT("EnemyCharacter is not valid in UpdateTargetInfoEvaluator"));
		return;
	}

	if (!InstanceData.EnemyAIController)
	{
		DEBUG_LOG(TEXT("EnemyAIController is not valid in UpdateTargetInfoEvaluator"));
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
	InstanceData.bInCombat = false;

	DEBUG_LOG(TEXT("UpdateTargetInfoEvaluator TreeStop"));
}

void FUpdateTargetInfoEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UpdateTargetInfo(Context);
}

void FUpdateTargetInfoEvaluator::UpdateTargetInfo(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bInCombat = InstanceData.EnemyAIController && InstanceData.EnemyAIController->IsInCombat();

	if (!InstanceData.EnemyAIController || !InstanceData.EnemyAIController->CurrentTarget.IsValid() || !InstanceData.EnemyCharacter)
	{
		InstanceData.DetectedTarget = nullptr;
		InstanceData.DistanceToTarget = -1.0f;
		InstanceData.AngleToTarget = 0.0f;
		InstanceData.bTargetDetected = false;
		return;
	}

	AActor* TargetActor = InstanceData.EnemyAIController->GetCurrentTargetActor();
	if (!TargetActor)
	{
		InstanceData.DetectedTarget = nullptr;
		InstanceData.DistanceToTarget = -1.0f;
		InstanceData.AngleToTarget = 0.0f;
		InstanceData.bTargetDetected = false;
		return;
	}

	InstanceData.DetectedTarget = TargetActor;

	const FVector SourceLocation = InstanceData.EnemyCharacter->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	//거리 계산 (수평면 기준. 3D면 키 차이만큼 길어져 큰 적일수록 거리 기준이 어긋남)
	const float Distance = FVector::Dist2D(SourceLocation, TargetLocation);
	InstanceData.DistanceToTarget = Distance;

	//각도 계산 (Enemy의 정면 기준 Target과의 Yaw 각도)
	//수평면 기준. 3D로 계산하면 키 차이(캡슐 중심 높이 차)만큼의 내려다보는 각이 섞여 정면이어도 0이 되지 않고 좌우 부호가 흔들림
	const FVector DirectionToTarget = (TargetLocation - SourceLocation).GetSafeNormal2D();
	const FVector ForwardVector = InstanceData.EnemyCharacter->GetActorForwardVector().GetSafeNormal2D();

	//내적을 이용한 각도 계산 (0~180도, 아래 외적으로 부호 부여)
	const float DotProduct = FMath::Clamp(FVector::DotProduct(ForwardVector, DirectionToTarget), -1.0f, 1.0f);
	const float AngleRadians = FMath::Acos(DotProduct);
	const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

	//외적을 이용하여 좌우 방향 판별 (음수면 왼쪽, 양수면 오른쪽)
	const FVector CrossProduct = FVector::CrossProduct(ForwardVector, DirectionToTarget);
	const float SignedAngle = (CrossProduct.Z < 0.0f) ? -AngleDegrees : AngleDegrees;

	InstanceData.AngleToTarget = SignedAngle;
	InstanceData.bTargetDetected = true;

	//EnemyAIController의 CurrentTarget 구조체 갱신
	InstanceData.EnemyAIController->CurrentTarget.Distance = Distance;
	InstanceData.EnemyAIController->CurrentTarget.AngleToTarget = SignedAngle;

	DEBUG_LOG(TEXT("Target Info Updated - Distance: %.2f, Angle: %.2f"), Distance, SignedAngle);
}
