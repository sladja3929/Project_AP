#include "GAS/Abilities/Player/RollAbility.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GameplayEffect.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogRollAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogRollAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

URollAbility::URollAbility()
{
	bRetriggerInstancedAbility = true;
	StaminaCost = 20.0f;
	RotateTime = 0.05f;
	bIgnoreLockOn = true;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void URollAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EventNotifyInvincibleStartTag = UGameplayTagsSubsystem::GetEventNotifyInvincibleStartTag();
	EffectInvincibilityDurationTag = UGameplayTagsSubsystem::GetEffectInvincibilityDurationTag();
	EffectJustRolledDurationTag = UGameplayTagsSubsystem::GetEffectJustRolledDurationTag();

	if (!EventNotifyInvincibleStartTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyInvincibleStartTag is not valid"));
	}
	if (!EffectInvincibilityDurationTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectInvincibilityDurationTag is not valid"));
	}
	if (!EffectJustRolledDurationTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectJustRolledDurationTag is not valid"));
	}
}

void URollAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//스태미나 부족 시 어빌리티 종료는 호출자가 책임
	if (!ConsumeStamina())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();
	START_WAIT_EVENT_TASK(WaitInvincibleStartEventTask, EventNotifyInvincibleStartTag, OnEventInvincibleStart, nullptr, true, true);
}

bool URollAbility::ConsumeStamina()
{
	if (!ApplyStaminaCost())
	{
		DEBUG_LOG(TEXT("No Stamina"));
		return false;
	}

	return true;
}

UAnimMontage* URollAbility::SetMontageToPlayTask()
{
	return RollMontage;
}

void URollAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}

	Super::SetUpPlayMontageWithEventsTask();
}

void URollAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("Montage Task Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URollAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Montage Task Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URollAbility::ApplyInvincibilityEffect()
{
	if (!InvincibilityEffect)
	{
		DEBUG_LOG(TEXT("No InvincibilityEffect set in Blueprint"));
		return;
	}

	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("No APASC"));
		return;
	}

	// 무적 이펙트 적용
	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle EffectSpec = APASC->CreateGameplayEffectSpec(InvincibilityEffect, EffectiveLevel, this);

	if (!EffectSpec.IsValid())
	{
		DEBUG_LOG(TEXT("Failed to create Invincibility Effect Spec"));
		return;
	}

	APASC->SetSpecSetByCallerMagnitude(EffectSpec, EffectInvincibilityDurationTag, InvincibilityDuration);
	InvincibilityEffectHandle = APASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	DEBUG_LOG(TEXT("Invincibility Effect Applied with Duration: %f"), InvincibilityDuration)
}

void URollAbility::ProcessActionRecoveryEnd()
{
	//JustRolled 태그 부여
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("No APASC"));
		Super::ProcessActionRecoveryEnd();
		return;
	}

	if (!JustRolledWindowEffect)
	{
		DEBUG_LOG(TEXT("No JustRolledWindowEffect"));
		Super::ProcessActionRecoveryEnd();
		return;
	}

	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle EffectSpec = APASC->CreateGameplayEffectSpec(JustRolledWindowEffect, EffectiveLevel, this);

	if (!EffectSpec.IsValid())
	{
		DEBUG_LOG(TEXT("Failed to create JustRolled Effect Spec"));
		Super::ProcessActionRecoveryEnd();
		return;
	}

	APASC->SetSpecSetByCallerMagnitude(EffectSpec, EffectJustRolledDurationTag, JustRolledWindowDuration);
	APASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	DEBUG_LOG(TEXT("JustRolled EffectWindow Attached"));

	Super::ProcessActionRecoveryEnd();
}

void URollAbility::OnEventInvincibleStart(FGameplayEventData Payload)
{
	DEBUG_LOG(TEXT("Invincible Start - Event Received"));
	ApplyInvincibilityEffect();
}

void URollAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	
	DEBUG_LOG(TEXT("Roll Ability End"));
	// 무적 이펙트 제거
	if (InvincibilityEffectHandle.IsValid())
	{
		if (ASC)
		{
			ASC->RemoveActiveGameplayEffect(InvincibilityEffectHandle);
			InvincibilityEffectHandle = FActiveGameplayEffectHandle();
		}
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
