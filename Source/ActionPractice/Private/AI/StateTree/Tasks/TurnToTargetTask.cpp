#include "AI/StateTree/Tasks/TurnToTargetTask.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimMontage.h"
#include "Characters/BaseCharacter.h"
#include "Characters/APMotionWarpingComponent.h"
#include "AI/EnemyAIController.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTurnToTargetTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogTurnToTargetTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

namespace
{
	ABaseCharacter* GetAvatarCharacter(const UAbilitySystemComponent* ASC)
	{
		return ASC ? Cast<ABaseCharacter>(ASC->GetAvatarActor()) : nullptr;
	}
}

EStateTreeRunStatus FTurnToTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.ElapsedTime = 0.0f;
	InstanceData.PlayingMontage = nullptr;
	InstanceData.bWarpTargetSet = false;

	UEnemyAbilitySystemComponent* ASC = InstanceData.EnemyAbilitySystemComponent;
	ABaseCharacter* Character = GetAvatarCharacter(ASC);

	if (!ASC || !Character || !InstanceData.EnemyAIController)
	{
		DEBUG_LOG(TEXT("EnterState: ASC(%p), Character(%p) or AIController(%p) is null"), ASC, Character, InstanceData.EnemyAIController.Get());
		return EStateTreeRunStatus::Failed;
	}

	const FCurrentTarget& TargetInfo = InstanceData.EnemyAIController->GetCurrentTarget();

	if (!TargetInfo.IsValid())
	{
		DEBUG_LOG(TEXT("EnterState: No valid target"));
		return EStateTreeRunStatus::Failed;
	}

	const float Angle = TargetInfo.AngleToTarget;

	if (FMath::Abs(Angle) <= InstanceData.ExitAngle)
	{
		DEBUG_LOG(TEXT("EnterState: Already facing target (Angle=%.1f)"), Angle);
		return EStateTreeRunStatus::Succeeded;
	}

	//양수 오른쪽, 음수 왼쪽 (UpdateTargetInfoEvaluator 기준)
	UAnimMontage* TurnMontage = (Angle > 0.0f) ? InstanceData.TurnRightMontage : InstanceData.TurnLeftMontage;

	if (!TurnMontage)
	{
		DEBUG_LOG(TEXT("EnterState: Walk turn (Angle=%.1f)"), Angle);
		return EStateTreeRunStatus::Running;
	}

	//몽타주 시작 전에 워프 타깃을 세팅해야 첫 워프 윈도우부터 반영됨
	if (UAPMotionWarpingComponent* MotionWarpingComponent = Character->GetMotionWarpingComponent())
	{
		MotionWarpingComponent->SetWarpTargetFollowActor(UAPMotionWarpingComponent::WarpTargetName_TurnTarget, TargetInfo.Actor.Get());
		InstanceData.bWarpTargetSet = true;
	}

	//어빌리티 없이 ASC로 재생 (PlayMontageTask와 같은 RepAnimMontageInfo 복제 경로)
	const float PlayLength = ASC->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), TurnMontage, InstanceData.PlayRate);

	if (PlayLength <= 0.0f)
	{
		DEBUG_LOG(TEXT("EnterState: PlayMontage failed. Montage=%s"), *GetNameSafe(TurnMontage));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PlayingMontage = TurnMontage;

	DEBUG_LOG(TEXT("EnterState: Playing %s (Angle=%.1f)"), *GetNameSafe(TurnMontage), Angle);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTurnToTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UEnemyAbilitySystemComponent* ASC = InstanceData.EnemyAbilitySystemComponent;
	ABaseCharacter* Character = GetAvatarCharacter(ASC);

	if (!ASC || !Character || !InstanceData.EnemyAIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FCurrentTarget& TargetInfo = InstanceData.EnemyAIController->GetCurrentTarget();

	if (!TargetInfo.IsValid())
	{
		DEBUG_LOG(TEXT("Tick: Target lost - Task FAILED"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;

	const float Angle = TargetInfo.AngleToTarget;

	//회전 완료 (몽타주 정지와 워프 타깃 정리는 ExitState). 같은 틱에 Timeout과 겹치면 완료 우선
	if (FMath::Abs(Angle) <= InstanceData.ExitAngle)
	{
		DEBUG_LOG(TEXT("Tick: Facing target (Angle=%.1f, %.2fs) - Task SUCCEEDED"), Angle, InstanceData.ElapsedTime);
		return EStateTreeRunStatus::Succeeded;
	}

	if (InstanceData.ElapsedTime >= InstanceData.Timeout)
	{
		DEBUG_LOG(TEXT("Tick: Timeout (Angle=%.1f) - Task FAILED"), Angle);
		return EStateTreeRunStatus::Failed;
	}

	//몽타주 회전: 몽타주 종료 또는 다른 몽타주(피격 등)로 대체되면 완료 처리 (남은 각도는 재선택에서 다시 판단)
	if (InstanceData.PlayingMontage)
	{
		if (ASC->GetCurrentMontage() != InstanceData.PlayingMontage)
		{
			DEBUG_LOG(TEXT("Tick: %s is no longer playing (Angle=%.1f) - Task SUCCEEDED"), *GetNameSafe(InstanceData.PlayingMontage), Angle);
			return EStateTreeRunStatus::Succeeded;
		}

		return EStateTreeRunStatus::Running;
	}

	//걸으며 회전: 정면에서 타깃 쪽으로 일부만 꺾은 방향으로 입력 (타깃 방향 그대로 넣으면 정면을 유지한 채 옆·뒤로 미끄러짐)
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const float InputYaw = FMath::Clamp(Angle, -InstanceData.MaxInputAngle, InstanceData.MaxInputAngle);
	const FVector InputDirection = FRotator(0.0f, InputYaw, 0.0f).RotateVector(Forward);

	Character->AddMovementInput(InputDirection, InstanceData.WalkInputScale);

	return EStateTreeRunStatus::Running;
}

void FTurnToTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UEnemyAbilitySystemComponent* ASC = InstanceData.EnemyAbilitySystemComponent;

	//내가 재생한 몽타주가 아직 재생 중일 때만 정지 (블렌드 아웃)
	if (ASC && InstanceData.PlayingMontage && ASC->GetCurrentMontage() == InstanceData.PlayingMontage)
	{
		ASC->CurrentMontageStop();
		DEBUG_LOG(TEXT("ExitState: Stopped %s"), *GetNameSafe(InstanceData.PlayingMontage));
	}

	InstanceData.PlayingMontage = nullptr;

	//직접 세팅한 워프 타깃만 정리
	if (InstanceData.bWarpTargetSet)
	{
		if (ABaseCharacter* Character = GetAvatarCharacter(ASC))
		{
			if (UAPMotionWarpingComponent* MotionWarpingComponent = Character->GetMotionWarpingComponent())
			{
				MotionWarpingComponent->RemoveWarpTarget(UAPMotionWarpingComponent::WarpTargetName_TurnTarget);
			}
		}

		InstanceData.bWarpTargetSet = false;
	}
}
