#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "GAS/Abilities/HitReactionProcessor.h"
#include "HitReactionAbility.generated.h"

class UBlockAbility;

UCLASS()
class ACTIONPRACTICE_API UHitReactionAbility : public UActionRecoveryAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UHitReactionAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReaction")
	TObjectPtr<UAnimMontage> HitReactionLightMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReaction")
	TObjectPtr<UAnimMontage> HitReactionMiddleMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReaction")
	TObjectPtr<UAnimMontage> HitReactionHeavyMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction")
	FHitReactionProcessor ReactionProcessor;

	FGameplayTag StateAbilityBlockingTag;
	FGameplayTag StateGuardBrokenTag;
	FGameplayTag AbilityBlockTag;
	FGameplayTag StateStaminaRegenBlockedTag;

	bool bIsBlockReaction = false;
	bool bIsGuardBreakReaction = false;

#pragma endregion

#pragma region "Protected Functions"

	virtual bool ConsumeStamina() override;

	virtual UAnimMontage* SetMontageToPlayTask() override;

	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskMontageInterrupted() override;

#pragma endregion

private:
#pragma region "Private Variables"

	//BlockReaction 동안 직접 부여한 리젠 차단 태그 보유 여부 (EndAbility 중복 호출 시 중복 제거 방지)
	bool bAddedStaminaRegenBlockedTag = false;

#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};