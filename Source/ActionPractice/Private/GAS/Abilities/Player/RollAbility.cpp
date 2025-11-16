#include "GAS/Abilities/Player/RollAbility.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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

UAnimMontage* URollAbility::SetMontageToPlayTask()
{
	return RollMontage;
}

void URollAbility::BindEventsAndReadyMontageTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}

	//Invincibility 노티파이 이벤트 바인딩
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyInvincibleStartTag);
	
	Super::BindEventsAndReadyMontageTask();
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

void URollAbility::OnTaskNotifyEventsReceived(FGameplayEventData Payload)
{
	Super::OnTaskNotifyEventsReceived(Payload);
	
	if (Payload.EventTag == EventNotifyInvincibleStartTag) OnNotifyInvincibleStart(Payload);
}

void URollAbility::OnEventActionRecoveryEnd(FGameplayEventData Payload)
{
	//JustRolled 태그 부여
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("No APASC"));
		Super::OnEventActionRecoveryEnd(Payload);
		return;
	}

	if (!JustRolledWindowEffect)
	{
		DEBUG_LOG(TEXT("No JustRolledWindowEffect"));
		Super::OnEventActionRecoveryEnd(Payload);
		return;
	}

	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle EffectSpec = APASC->CreateGameplayEffectSpec(JustRolledWindowEffect, EffectiveLevel, this);

	if (!EffectSpec.IsValid())
	{
		DEBUG_LOG(TEXT("Failed to create JustRolled Effect Spec"));
		Super::OnEventActionRecoveryEnd(Payload);
		return;
	}

	APASC->SetSpecSetByCallerMagnitude(EffectSpec, EffectJustRolledDurationTag, JustRolledWindowDuration);
	APASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	DEBUG_LOG(TEXT("JustRolled EffectWindow Attached"));
	
	Super::OnEventActionRecoveryEnd(Payload);
}

void URollAbility::OnNotifyInvincibleStart(FGameplayEventData Payload)
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
