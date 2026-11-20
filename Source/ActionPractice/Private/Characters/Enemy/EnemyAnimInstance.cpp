#include "Characters/Enemy/EnemyAnimInstance.h"
#include "Characters/EnemyCharacter.h"
#include "Animation/AnimMontage.h"
#include "Notifies/AnimNotifyState_LookAtTarget.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAnimInstance, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAnimInstance, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerEnemy = Cast<AEnemyCharacter>(TryGetPawnOwner());
	CurrentLookYaw = 0.0f;
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerEnemy.IsValid())
	{
		OwnerEnemy = Cast<AEnemyCharacter>(TryGetPawnOwner());
	}

	CurrentLookYaw = FMath::FInterpTo(CurrentLookYaw, ComputeTargetLookYaw(), DeltaSeconds, LookInterpSpeed);

	//양수 오른쪽. 메시가 캐릭터 기준으로 회전되어 있어도 컴포넌트 공간 Z축 회전은 수평 회전이라 부호만 유지됨
	NeckLookRotation = FRotator(0.0f, CurrentLookYaw * NeckShare, 0.0f);
	HeadLookRotation = FRotator(0.0f, CurrentLookYaw * (1.0f - NeckShare), 0.0f);
}

float UEnemyAnimInstance::ComputeTargetLookYaw()
{
	const AEnemyCharacter* Enemy = OwnerEnemy.Get();
	const AActor* Target = Enemy ? Enemy->GetCombatTarget() : nullptr;

	if (!Target)
	{
		bLookingByAngle = false;
		return 0.0f;
	}

	//몽타주 재생 중에는 Look At Target ANS 구간에서만 허용 (그 외 몽타주는 몸·머리 방향을 직접 다룸)
	if (GetCurrentActiveMontage() && !IsLookAtNotifyActive())
	{
		bLookingByAngle = false;
		return 0.0f;
	}

	const FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();

	if (Forward.IsNearlyZero() || ToTarget.IsNearlyZero())
	{
		return 0.0f;
	}

	//수평 부호 각도 (UpdateTargetInfoEvaluator와 같은 기준, 양수 오른쪽)
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToTarget), -1.0f, 1.0f)));
	const float SignedAngle = FVector::CrossProduct(Forward, ToTarget).Z < 0.0f ? -AngleDegrees : AngleDegrees;

	//바라보는 중에는 MaxLookAngle에서 해제, 해제 중에는 그보다 LookAngleHysteresis 안쪽에서 재개
	const float ThresholdAngle = bLookingByAngle ? MaxLookAngle : MaxLookAngle - LookAngleHysteresis;
	bLookingByAngle = FMath::Abs(SignedAngle) <= ThresholdAngle;

	if (!bLookingByAngle)
	{
		return 0.0f;
	}

	return FMath::Clamp(SignedAngle, -MaxLookYaw, MaxLookYaw);
}

bool UEnemyAnimInstance::IsLookAtNotifyActive() const
{
	for (const FAnimNotifyEvent& NotifyEvent : ActiveAnimNotifyState)
	{
		if (Cast<UAnimNotifyState_LookAtTarget>(NotifyEvent.NotifyStateClass))
		{
			return true;
		}
	}

	return false;
}
