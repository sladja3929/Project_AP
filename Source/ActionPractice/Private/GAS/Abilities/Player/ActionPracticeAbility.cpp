#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeGameplayAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeGameplayAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UActionPracticeAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
}

bool UActionPracticeAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UActionPracticeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UActionPracticeAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();
}

UActionPracticeAbilitySystemComponent* UActionPracticeAbility::GetActionPracticeAbilitySystemComponentFromActorInfo() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		return Cast<UActionPracticeAbilitySystemComponent>(ASC);
	}
	return nullptr;
}

AActionPracticeCharacter* UActionPracticeAbility::GetActionPracticeCharacterFromActorInfo() const
{
	return Cast<AActionPracticeCharacter>(GetActorInfo().AvatarActor.Get());
}

AActionPracticeCharacter* UActionPracticeAbility::GetActionPracticeCharacterFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<AActionPracticeCharacter>(ActorInfo->AvatarActor.Get());
}

UActionPracticeAttributeSet* UActionPracticeAbility::GetActionPracticeAttributeSetFromActorInfo() const
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (Character)
	{
		return Character->GetAttributeSet();
	}
	return nullptr;
}

UActionPracticeAttributeSet* UActionPracticeAbility::GetActionPracticeAttributeSetFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo(ActorInfo);
	if (Character)
	{
		return Character->GetAttributeSet();
	}
	return nullptr;
}

UInputBufferComponent* UActionPracticeAbility::GetInputBufferComponentFromActorInfo() const
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (Character)
	{
		return Character->GetInputBufferComponent();
	}
	return nullptr;
}

void UActionPracticeAbility::StartWaitInputReleaseTask(bool bTestAlreadyReleased)
{
	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, bTestAlreadyReleased);
	if (!WaitInputReleaseTask)
		return;

	WaitInputReleaseTask->OnRelease.AddDynamic(this, &UActionPracticeAbility::OnWaitInputRelease);
	WaitInputReleaseTask->ReadyForActivation();
}

void UActionPracticeAbility::OnWaitInputRelease(float TimeHeld)
{
	//Base는 기본 동작 없음 (각 Ability에서 override)
}

void UActionPracticeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}