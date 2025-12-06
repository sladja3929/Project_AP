#include "GAS/Abilities/Enemy/EnemyAttackAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/BossCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "GAS/AbilitySystemComponent/BossAbilitySystemComponent.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Items/AttackData.h"
#include "AI/EnemyAIController.h"
#include "Characters/ActionPracticeCharacter.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEnemyAttackAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EventNotifyRotateToTargetTag = UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTag();
	EventNotifyCheckConditionTag = UGameplayTagsSubsystem::GetEventNotifyCheckConditionTag();
	EventNotifyActionRecoveryEndTag = UGameplayTagsSubsystem::GetEventNotifyActionRecoveryEndTag();

	if (!EventNotifyRotateToTargetTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyRotateToTargetTag is not valid"));
	}

	if (!EventNotifyCheckConditionTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyCheckConditionTag is not valid"));
	}

	if (!EventNotifyActionRecoveryEndTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyActionRecoveryEndTag is not valid"));
	}
}

void UEnemyAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("EnemyAttackAbility::ActivateAbility FAIL - BossCharacter is nullptr. Ability=%s"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!HitDetectionSetter.Init(BossCharacter->GetHitDetectionInterface()))
	{
		DEBUG_LOG(TEXT("EnemyAttackAbility::ActivateAbility FAIL - HitDetectionSetter.Init failed. Ability=%s"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!HitDetectionSetter.Bind(this))
	{
		DEBUG_LOG(TEXT("EnemyAttackAbility::ActivateAbility FAIL - HitDetectionSetter.Bind failed. Ability=%s"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const UEnemyDataAsset* EnemyData = BossCharacter->GetEnemyData();
	if (!EnemyData)
	{
		DEBUG_LOG(TEXT("EnemyAttackAbility::ActivateAbility FAIL - EnemyData is nullptr. Ability=%s"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	EnemyAttackData = EnemyData->NamedAttackData.Find(AttackName);
	if (!EnemyAttackData || EnemyAttackData->ComboSequence.Num() == 0)
	{
		DEBUG_LOG(TEXT("ActivateAbility: Attack data not found for name: %s"), *AttackName.ToString());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	MaxComboCount = EnemyAttackData->ComboSequence.Num();

	//Ability 시작 시 AIController로부터 CurrentTarget 정보 캐싱
	AEnemyAIController* AIController = GetEnemyAIControllerFromActorInfo();
	if (AIController)
	{
		CachedTargetInfo = AIController->GetCurrentTarget();
	}

	ComboCounter = 0;
	bPerformNextCombo = true;
	bCreateTask = true;
	PlayAction();
}

void UEnemyAttackAbility::SetHitDetectionConfig()
{
	//PrepareHitDetection 호출
	if (!HitDetectionSetter.PrepareHitDetection(AttackName, ComboCounter))
	{
		DEBUG_LOG(TEXT("Failed to prepare HitDetection"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	DEBUG_LOG(TEXT("Attack Ability: Call Hit Detection Prepare"));
}

void UEnemyAttackAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
	//Source ASC (공격자, AttackAbility 소유자)
	UBossAbilitySystemComponent* SourceASC = GetBossAbilitySystemComponentFromActorInfo();
	if (!HitActor || !SourceASC) return;

	//Target ASC (피격자)
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC) return;

	//Source ASC에서 GE Spec 생성
	FGameplayEffectSpecHandle SpecHandle = SourceASC->CreateAttackGameplayEffectSpec(DamageInstantEffect, GetAbilityLevel(), this, AttackData);

	if (SpecHandle.IsValid())
	{
		//Target에게 적용
		FActiveGameplayEffectHandle ActiveGEHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		//적용에 성공했으면
		if (ActiveGEHandle.WasSuccessfullyApplied())
		{
			DEBUG_LOG(TEXT("Damage Applied Successfully"));
		}
	}
}

void UEnemyAttackAbility::PlayAction()
{
	SetHitDetectionConfig();

	ExecuteMontageTask();
}

UAnimMontage* UEnemyAttackAbility::SetMontageToPlayTask()
{
	if (!EnemyAttackData)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No EnemyAttackData"));
		return nullptr;
	}

	if (ComboCounter < 0)
	{
		ComboCounter = 0;
	}

	// 소프트 레퍼런스를 실제 오브젝트로 로드
	const auto& ComboData = EnemyAttackData->ComboSequence[ComboCounter];
	UAnimMontage* Montage = ComboData.AttackMontage.LoadSynchronous();
	if (!Montage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load montage. AttackName=%s, ComboIndex=%d"),
			*AttackName.ToString(), ComboCounter);
		return nullptr;
	}

	return Montage;
}

void UEnemyAttackAbility::ExecuteMontageTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("EnemyAttackAbility::ExecuteMontageTask FAIL - MontageToPlay is nullptr. Ability=%s"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (bCreateTask) //커스텀 태스크 생성
	{
		PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
			this,
			NAME_None,
			MontageToPlay,
			1.0f,
			NAME_None,
			1.0f
		);

		BindEventsAndReadyMontageTask();
	}
	else //태스크 중간에 몽타주 바꾸기
	{
		PlayMontageWithEventsTask->ChangeMontageAndPlay(MontageToPlay);
	}
}

void UEnemyAttackAbility::BindEventsAndReadyMontageTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//커스텀 몽타주 태스크 델리게이트 바인딩
	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UEnemyAttackAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UEnemyAttackAbility::OnTaskMontageInterrupted);
	PlayMontageWithEventsTask->OnNotifyEventsReceived.AddDynamic(this, &UEnemyAttackAbility::OnTaskNotifyEventsReceived);

	//노티파이 이벤트 바인딩
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyRotateToTargetTag);
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyCheckConditionTag);
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyActionRecoveryEndTag);

	//태스크 활성화
	PlayMontageWithEventsTask->ReadyForActivation();
}

void UEnemyAttackAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("Montage Task Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEnemyAttackAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Montage Task Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UEnemyAttackAbility::OnTaskNotifyEventsReceived(FGameplayEventData Payload)
{
	if (Payload.EventTag == EventNotifyRotateToTargetTag)
	{
		OnEventRotateToTarget(Payload);
	}
	else if (Payload.EventTag == EventNotifyCheckConditionTag)
	{
		OnEventCheckCondition(Payload);
	}
	else if (Payload.EventTag == EventNotifyActionRecoveryEndTag)
	{
		OnEventActionRecoveryEnd(Payload);
	}
}

void UEnemyAttackAbility::OnEventRotateToTarget(FGameplayEventData Payload)
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No BossCharacter"));
		return;
	}

	//캐싱된 Target Actor 가져오기
	if (!CachedTargetInfo.IsValid())
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No Cached Target"));
		return;
	}

	AActor* TargetActor = CachedTargetInfo.Actor.Get();
	if (!TargetActor)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: Target Actor is nullptr"));
		return;
	}

	//타겟을 향해 회전
	BossCharacter->RotateToTarget(TargetActor, RotateTime);
	DEBUG_LOG(TEXT("OnEventRotateToTarget: Rotating to %s"),	*TargetActor->GetName());
}

void UEnemyAttackAbility::OnEventCheckCondition(FGameplayEventData Payload)
{
	AEnemyAIController* AIController = GetEnemyAIControllerFromActorInfo();
	if (!AIController)
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: No AIController"));
		bPerformNextCombo = false;
		return;
	}

	FCurrentTarget CurrentTargetInfo = AIController->GetCurrentTarget();
	if (!CurrentTargetInfo.IsValid())
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: No Valid Target"));
		bPerformNextCombo = false;
		return;
	}

	//거리 체크
	if (CurrentTargetInfo.Distance > MaxTargetDistance)
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: Target too far - Distance: %.2f, Max: %.2f"), CurrentTargetInfo.Distance, MaxTargetDistance);
		bPerformNextCombo = false;
		return;
	}

	//각도 체크 (절대값)
	if (FMath::Abs(CurrentTargetInfo.AngleToTarget) > MaxTargetAngle)
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: Target angle out of range - Angle: %.2f, Max: %.2f"), CurrentTargetInfo.AngleToTarget, MaxTargetAngle);
		bPerformNextCombo = false;
		return;
	}

	DEBUG_LOG(TEXT("OnEventCheckCondition: Passed - Distance: %.2f, Angle: %.2f"), CurrentTargetInfo.Distance, CurrentTargetInfo.AngleToTarget);
}

void UEnemyAttackAbility::OnEventActionRecoveryEnd(FGameplayEventData Payload)
{
	if (bPerformNextCombo)
	{
		DEBUG_LOG(TEXT("OnEventActionRecoveryEnd: Performing Next Combo"));
		PlayNextCombo();
	}
	else
	{
		DEBUG_LOG(TEXT("OnEventActionRecoveryEnd: Combo Cancelled"));
	}
}

void UEnemyAttackAbility::PlayNextCombo()
{
	++ComboCounter;

	//콤보 카운터가 콤보 시퀀스를 벗어나면 종료
	if (ComboCounter >= MaxComboCount)
	{
		DEBUG_LOG(TEXT("PlayNextCombo: Combo Finished - ComboCounter: %d"), ComboCounter);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	DEBUG_LOG(TEXT("PlayNextCombo: ComboCounter: %d"), ComboCounter);

	bPerformNextCombo = true;
	bCreateTask = false;
	PlayAction();
}

void UEnemyAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	DEBUG_LOG(TEXT("EndAbility %d"), bWasCancelled);

	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		//HitDetectionSetter 언바인딩
		HitDetectionSetter.UnBind();

		if (PlayMontageWithEventsTask)
		{
			PlayMontageWithEventsTask->bStopMontageWhenAbilityCancelled = bWasCancelled;
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}