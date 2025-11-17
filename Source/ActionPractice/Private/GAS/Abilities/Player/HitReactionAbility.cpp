#include "GAS/Abilities/Player/HitReactionAbility.h"
#include "Animation/AnimMontage.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Player/BlockAbility.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogHitReactionAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogHitReactionAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UHitReactionAbility::UHitReactionAbility()
{
	bRotateBeforeAction = false;
	StaminaCost = -1.0f; // 스태미나 체크 안함
}

void UHitReactionAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	StateAbilityBlockingTag = UGameplayTagsSubsystem::GetStateAbilityBlockingTag();
	if (!StateAbilityBlockingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityBlockingTag is Invalid"));
	}

	AbilityBlockTag = UGameplayTagsSubsystem::GetAbilityBlockTag();
	if (!AbilityBlockTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityBlockTag is Invalid"));
	}
}

void UHitReactionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bIsBlockReaction = false;

	if (TriggerEventData)
	{
		const float PoiseValue = TriggerEventData->EventMagnitude;
		DEBUG_LOG(TEXT("HitReaction activated with Poise=%.1f"), PoiseValue);

		//블로킹 상태 확인
		if (TriggerEventData->TargetTags.HasTag(StateAbilityBlockingTag))
		{
			bIsBlockReaction = true;
			DEBUG_LOG(TEXT("Block Reaction detected"));

			//BlockReaction 동안 State.Blocking 태그 수동 추가
			if (StateAbilityBlockingTag.IsValid())
			{
				ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(StateAbilityBlockingTag);
				DEBUG_LOG(TEXT("State.Blocking tag added for BlockReaction"));
			}
		}

		ReactionProcessor.SelectReactionLevel(PoiseValue);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

UAnimMontage* UHitReactionAbility::SetMontageToPlayTask()
{
	const EReactionLevel Level = ReactionProcessor.GetReactionLevel();

	if (bIsBlockReaction)
	{
		const FBlockActionData* BlockData = FWeaponAbilityStatics::GetBlockDataFromAbility(this);
		if (BlockData)
		{
			switch (Level)
			{
				case EReactionLevel::Heavy:
					DEBUG_LOG(TEXT("Playing BlockReactionHeavy"));
					return BlockData->BlockReactionHeavyMontage.LoadSynchronous();
				case EReactionLevel::Middle:
					DEBUG_LOG(TEXT("Playing BlockReactionMiddle"));
					return BlockData->BlockReactionMiddleMontage.LoadSynchronous();
				case EReactionLevel::Light:
					DEBUG_LOG(TEXT("Playing BlockReactionLight"));
					return BlockData->BlockReactionLightMontage.LoadSynchronous();
				default:
					return nullptr;
			}
		}
	}

	switch (Level)
	{
		case EReactionLevel::Heavy: return HitReactionHeavyMontage;
		case EReactionLevel::Middle: return HitReactionMiddleMontage;
		case EReactionLevel::Light: return HitReactionLightMontage;
		default: return nullptr;
	}
}

void UHitReactionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//BlockReaction 후처리, 누르고 있으면 계속 방어
	if (bIsBlockReaction)
	{
		//State.Blocking 태그 제거
		if (StateAbilityBlockingTag.IsValid())
		{
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(StateAbilityBlockingTag);
			DEBUG_LOG(TEXT("State.Blocking tag removed after BlockReaction"));
		}

		DEBUG_LOG(TEXT("bWasCancelled=%d"), bWasCancelled);

		//BlockAbility Spec 찾기
		FGameplayAbilitySpec* BlockAbilitySpec = nullptr;
		for (FGameplayAbilitySpec& Spec : GetAbilitySystemComponentFromActorInfo()->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(AbilityBlockTag))
			{
				BlockAbilitySpec = &Spec;
				break;
			}
		}

		if (BlockAbilitySpec)
		{
			//InputPressed로 입력이 눌려있는지 확인
			const bool bIsBlockInputPressed = BlockAbilitySpec->InputPressed > 0;
			DEBUG_LOG(TEXT("bIsBlockInputPressed=%d"), bIsBlockInputPressed);

			if (bIsBlockInputPressed)
			{
				DEBUG_LOG(TEXT("Block input still pressed, reactivating BlockAbility"));
				ActorInfo->AbilitySystemComponent->TryActivateAbility(BlockAbilitySpec->Handle);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}