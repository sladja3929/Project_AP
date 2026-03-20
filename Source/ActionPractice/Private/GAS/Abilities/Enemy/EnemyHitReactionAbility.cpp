#include "GAS/Abilities/Enemy/EnemyHitReactionAbility.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyHitReactionAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyHitReactionAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Constructor + Initialization"
UEnemyHitReactionAbility::UEnemyHitReactionAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UEnemyHitReactionAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	StateHitReactionTag = UGameplayTagsSubsystem::GetStateHitReactionTag();

	//EnemyDataAsset에서 몽타주 캐싱
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo(ActorInfo);
	if (Enemy)
	{
		const UEnemyDataAsset* EnemyData = Enemy->GetEnemyData();
		if (EnemyData)
		{
			if (!EnemyData->HitReactionLightMontage.IsNull())
			{
				CachedHitReactionLightMontage = EnemyData->HitReactionLightMontage.LoadSynchronous();
			}
			if (!EnemyData->HitReactionMiddleMontage.IsNull())
			{
				CachedHitReactionMiddleMontage = EnemyData->HitReactionMiddleMontage.LoadSynchronous();
			}
			if (!EnemyData->HitReactionHeavyMontage.IsNull())
			{
				CachedHitReactionHeavyMontage = EnemyData->HitReactionHeavyMontage.LoadSynchronous();
			}
		}
	}
}
#pragma endregion

#pragma region "Ability Lifecycle"
void UEnemyHitReactionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//상태 태그 추가 (StateTree 전이 트리거)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateHitReactionTag.IsValid())
	{
		ASC->AddLooseGameplayTag(StateHitReactionTag);
	}

	//Poise 음수값으로 리액션 레벨 결정
	if (TriggerEventData)
	{
		const float PoiseValue = TriggerEventData->EventMagnitude;
		ReactionProcessor.SelectReactionLevel(PoiseValue);
		DEBUG_LOG(TEXT("EnemyHitReaction activated with Poise=%.1f, Level=%d"), PoiseValue, (int32)ReactionProcessor.GetReactionLevel());
	}

	StartMontageWithEventsTask();
}

void UEnemyHitReactionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//상태 태그 제거
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateHitReactionTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(StateHitReactionTag);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
#pragma endregion

#pragma region "IMontageAbilityInterface"
UAnimMontage* UEnemyHitReactionAbility::SetMontageToPlayTask()
{
	switch (ReactionProcessor.GetReactionLevel())
	{
	case EReactionLevel::Heavy: return CachedHitReactionHeavyMontage;
	case EReactionLevel::Middle: return CachedHitReactionMiddleMontage;
	case EReactionLevel::Light: return CachedHitReactionLightMontage;
	default: return CachedHitReactionLightMontage;
	}
}

void UEnemyHitReactionAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("SetUpPlayMontageWithEventsTask: No task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UEnemyHitReactionAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UEnemyHitReactionAbility::OnTaskMontageInterrupted);
}

void UEnemyHitReactionAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("No HitReaction montage"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//기존 태스크 정리
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}

	PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		1.0f
	);

	SetUpPlayMontageWithEventsTask();
	PlayMontageWithEventsTask->ReadyForActivation();

	DEBUG_LOG(TEXT("EnemyHitReaction montage started: %s"), *MontageToPlay->GetName());
}

void UEnemyHitReactionAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("EnemyHitReaction montage completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEnemyHitReactionAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("EnemyHitReaction montage interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
#pragma endregion
