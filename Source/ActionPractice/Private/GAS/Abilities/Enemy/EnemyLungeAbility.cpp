#include "GAS/Abilities/Enemy/EnemyLungeAbility.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "AI/EnemyAIController.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Curves/CurveVector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyLungeAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyLungeAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UEnemyLungeAbility::UEnemyLungeAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

#pragma region "Activate Initialization"

void UEnemyLungeAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	CacheLungeConfig();
	CacheLungeTags();
}

void UEnemyLungeAbility::CacheLungeConfig()
{
	if (EnemyAttackData && EnemyAttackData->AttackType == EComboAttackType::Lunge)
	{
		CachedLungeConfig = &EnemyAttackData->LungeConfig;
		DEBUG_LOG(TEXT("CacheLungeConfig: HeightCurve=%s"),
			CachedLungeConfig->HeightCurve ? TEXT("Set") : TEXT("None"));
	}
	else
	{
		CachedLungeConfig = nullptr;
		DEBUG_LOG(TEXT("CacheLungeConfig: AttackType is not Lunge"));
	}
}

void UEnemyLungeAbility::CacheLungeTags()
{
	EventNotifyTrackingTargetTag = UGameplayTagsSubsystem::GetEventNotifyTrackingTargetTag();
	EventNotifyLungeStartTag = UGameplayTagsSubsystem::GetEventNotifyLungeStartTag();
	EventNotifyLungeEndTag = UGameplayTagsSubsystem::GetEventNotifyLungeEndTag();
}

#pragma endregion

#pragma region "Ability Lifecycle"

void UEnemyLungeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//부모의 ActivateAbility → ActivateInitSettings → ExecuteAttack 호출
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CachedLungeConfig)
	{
		DEBUG_LOG(TEXT("ActivateAbility: No CachedLungeConfig — ending ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//초기 타겟 위치 캐싱 (TrackingTarget ANS 시작 전 fallback)
	if (CachedTargetInfo.IsValid() && CachedTargetInfo.Actor.IsValid())
	{
		CachedDestination = CachedTargetInfo.Actor->GetActorLocation();
	}

	//ANS 이벤트 대기 태스크 등록
	START_WAIT_EVENT_TASK(WaitTrackingTargetEventTask, EventNotifyTrackingTargetTag, OnEventTrackingTarget, nullptr, false, true);
	START_WAIT_EVENT_TASK(WaitLungeStartEventTask, EventNotifyLungeStartTag, OnEventLungeStart, nullptr, true, true);
	START_WAIT_EVENT_TASK(WaitLungeEndEventTask, EventNotifyLungeEndTag, OnEventLungeEnd, nullptr, true, true);

	DEBUG_LOG(TEXT("ActivateAbility: Lunge started — MaxCombo=%d"), MaxComboCount);
}

void UEnemyLungeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//이동 태스크 정리
	if (LungeMovementTask)
	{
		LungeMovementTask->EndTask();
		LungeMovementTask = nullptr;
	}

	//이벤트 태스크 정리
	END_ABILITY_TASK(WaitTrackingTargetEventTask);
	END_ABILITY_TASK(WaitLungeStartEventTask);
	END_ABILITY_TASK(WaitLungeEndEventTask);

	//상태 초기화
	CachedLungeConfig = nullptr;
	CachedDestination = FVector::ZeroVector;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#pragma endregion

#pragma region "Event Handlers"

void UEnemyLungeAbility::OnEventTrackingTarget(FGameplayEventData Payload)
{
	if (Payload.Target)
	{
		CachedDestination = Payload.Target->GetActorLocation();
	}

	//이동 중이 아니면 위치 캐싱만
	if (!LungeMovementTask) return;

	//CMC에 등록된 RootMotionSource를 직접 갱신 (태스크 멤버는 protected라 접근 불가)
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy) return;

	UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement();
	if (!CMC) return;

	//태스크 생성 시 사용한 TaskInstanceName("LungeMove")으로 소스 탐색
	for (TSharedPtr<FRootMotionSource>& Source : CMC->CurrentRootMotion.RootMotionSources)
	{
		if (Source.IsValid() && Source->InstanceName == FName("LungeMove"))
		{
			FRootMotionSource_MoveToForce* MoveToForce = static_cast<FRootMotionSource_MoveToForce*>(Source.Get());
			MoveToForce->TargetLocation = CachedDestination;
			break;
		}
	}
}

void UEnemyLungeAbility::OnEventLungeStart(FGameplayEventData Payload)
{
	//Lunging ANS Begin — EventMagnitude = ANS 지속시간 (이동 시간)
	float Duration = Payload.EventMagnitude;
	DEBUG_LOG(TEXT("OnEventLungeStart: Duration=%.2f, Destination=%s"), Duration, *CachedDestination.ToString());

	StartLungeMovement(Duration);
}

void UEnemyLungeAbility::OnEventLungeEnd(FGameplayEventData Payload)
{
	//Lunging ANS End — 이동 태스크 정리
	DEBUG_LOG(TEXT("OnEventLungeEnd"));

	if (LungeMovementTask)
	{
		LungeMovementTask->EndTask();
		LungeMovementTask = nullptr;
	}
}

void UEnemyLungeAbility::OnLungeMovementFinished()
{
	//MoveToForce 태스크 시간 만료 — 이동 태스크 정리
	DEBUG_LOG(TEXT("OnLungeMovementFinished"));

	if (LungeMovementTask)
	{
		LungeMovementTask->EndTask();
		LungeMovementTask = nullptr;
	}
}

#pragma endregion

#pragma region "Movement"

void UEnemyLungeAbility::StartLungeMovement(float Duration)
{
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy || !CachedLungeConfig)
	{
		DEBUG_LOG(TEXT("StartLungeMovement: Enemy or LungeConfig is nullptr"));
		return;
	}

	Duration = FMath::Max(Duration, 0.05f);

	DEBUG_LOG(TEXT("StartLungeMovement: Start=%s, Dest=%s, Duration=%.2f"),
		*Enemy->GetActorLocation().ToString(),
		*CachedDestination.ToString(),
		Duration);

	LungeMovementTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this,
		FName("LungeMove"),
		CachedDestination,
		Duration,
		false,  //bSetNewMovementMode
		CachedLungeConfig->HeightCurve ? EMovementMode::MOVE_Falling : EMovementMode::MOVE_Walking,
		false,  //bRestrictSpeedToExpected
		CachedLungeConfig->HeightCurve.Get(), //PathOffsetCurve (UCurveVector*)
		ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector,
		0.0f
	);

	if (LungeMovementTask)
	{
		//시간 만료 + 목적지 도달, 또는 시간 만료만 — 둘 다 같은 콜백
		LungeMovementTask->OnTimedOutAndDestinationReached.AddDynamic(this, &UEnemyLungeAbility::OnLungeMovementFinished);
		LungeMovementTask->OnTimedOut.AddDynamic(this, &UEnemyLungeAbility::OnLungeMovementFinished);
		LungeMovementTask->ReadyForActivation();
		DEBUG_LOG(TEXT("StartLungeMovement: MoveToForce task started"));
	}
	else
	{
		DEBUG_LOG(TEXT("StartLungeMovement: Failed to create MoveToForce task"));
	}
}

#pragma endregion

#pragma region "Override Functions"

void UEnemyLungeAbility::OnActionRecoveryEnd()
{
	//Lunge 콤보(인덱스 0)는 CheckCondition AN이 없으므로
	//ComboCounter가 증가하지 않고 bPerformNextCombo가 초기값(true) 그대로 남음
	//무한 루프 방지를 위해 직접 ComboCounter를 증가시키고 후속 콤보 시도
	if (ComboCounter == 0)
	{
		ComboCounter++;

		if (ComboCounter < MaxComboCount)
		{
			DEBUG_LOG(TEXT("OnActionRecoveryEnd: Lunge done → starting follow-up combo (ComboCounter=%d)"), ComboCounter);
			bPerformNextCombo = true;
			ExecuteAttack();
		}
		else
		{
			DEBUG_LOG(TEXT("OnActionRecoveryEnd: Lunge done — no follow-up combos"));
			//후속 콤보 없음 → 몽타주 완료가 EndAbility 처리
		}
	}
	else
	{
		//후속 콤보(인덱스 1+): 부모의 일반 콤보 흐름 (CheckCondition 기반)
		DEBUG_LOG(TEXT("OnActionRecoveryEnd: Follow-up combo — delegating to parent"));
		Super::OnActionRecoveryEnd();
	}
}

#pragma endregion
