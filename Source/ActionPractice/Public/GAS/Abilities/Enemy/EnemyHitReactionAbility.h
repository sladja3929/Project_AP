#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "GAS/Abilities/HitReactionProcessor.h"
#include "EnemyHitReactionAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UEnemyDataAsset;

UCLASS()
class ACTIONPRACTICE_API UEnemyHitReactionAbility : public UEnemyAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UEnemyHitReactionAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	//FHitReactionProcessor 재사용 (플레이어와 동일한 Poise 음수값 기반 분류)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction")
	FHitReactionProcessor ReactionProcessor;

	//태그
	FGameplayTag StateHitReactionTag;

#pragma endregion

#pragma region "Protected Functions"

	// ===== IMontageAbilityInterface =====
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;
	virtual void StartMontageWithEventsTask() override;

	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
