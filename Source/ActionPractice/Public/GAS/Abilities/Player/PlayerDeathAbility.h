#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "PlayerDeathAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UAbilityTask_WaitDelay;

UCLASS()
class ACTIONPRACTICE_API UPlayerDeathAbility : public UActionPracticeAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UPlayerDeathAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	float RespawnDelay = 3.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	TSubclassOf<UGameplayEffect> RespawnRecoveryEffect;

	//태스크
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WaitDelayTask = nullptr;

	//태그
	FGameplayTag AbilityDeathTag;
	FGameplayTag StateDeadTag;

#pragma endregion

#pragma region "Protected Functions"

	//===== IMontageAbilityInterface =====
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;
	virtual void StartMontageWithEventsTask() override;

	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	//===== 흐름 제어 =====
	void StartRespawnDelay();
	void PerformRespawn();

	//===== 헬퍼 =====
	void AddStateDeadTag();
	void DisableCharacterMovement();
	void RestoreCharacterMovement();
	void ApplyRespawnRecovery();
	void RequestEnemyReset();

	UFUNCTION()
	void OnRespawnDelayFinished();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
