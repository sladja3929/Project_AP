#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/BossCharacter.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "AI/EnemyAIController.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEnemyAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
}

bool UEnemyAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UEnemyAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UEnemyAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemyAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();
}

AEnemyCharacter* UEnemyAbility::GetEnemyCharacterFromActorInfo() const
{
	return Cast<AEnemyCharacter>(GetActorInfo().AvatarActor.Get());
}

AEnemyCharacter* UEnemyAbility::GetEnemyCharacterFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get());
}

UEnemyAbilitySystemComponent* UEnemyAbility::GetEnemyAbilitySystemComponentFromActorInfo() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		return Cast<UEnemyAbilitySystemComponent>(ASC);
	}
	return nullptr;
}

ABossCharacter* UEnemyAbility::GetBossCharacterFromActorInfo() const
{
	return Cast<ABossCharacter>(GetActorInfo().AvatarActor.Get());
}

ABossCharacter* UEnemyAbility::GetBossCharacterFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<ABossCharacter>(ActorInfo->AvatarActor.Get());
}

UEnemyAttributeSet* UEnemyAbility::GetEnemyAttributeSetFromActorInfo() const
{
	AEnemyCharacter* Character = GetEnemyCharacterFromActorInfo();
	if (Character)
	{
		return Character->GetAttributeSet();
	}
	return nullptr;
}

UEnemyAttributeSet* UEnemyAbility::GetEnemyAttributeSetFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AEnemyCharacter* Character = GetEnemyCharacterFromActorInfo(ActorInfo);
	if (Character)
	{
		return Character->GetAttributeSet();
	}
	return nullptr;
}

AEnemyAIController* UEnemyAbility::GetEnemyAIControllerFromActorInfo() const
{
	AEnemyCharacter* Character = GetEnemyCharacterFromActorInfo();
	if (Character)
	{
		return Character->GetEnemyAIController();
	}
	return nullptr;
}

AEnemyAIController* UEnemyAbility::GetEnemyAIControllerFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AEnemyCharacter* Character = GetEnemyCharacterFromActorInfo(ActorInfo);
	if (Character)
	{
		return Character->GetEnemyAIController();
	}
	return nullptr;
}