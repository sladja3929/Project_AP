#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "RestAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;
class ABonfire;

UCLASS()
class ACTIONPRACTICE_API URestAbility : public UActionPracticeAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	URestAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TObjectPtr<UAnimMontage> SitMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TObjectPtr<UAnimMontage> LoopMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TObjectPtr<UAnimMontage> StandMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TSubclassOf<UGameplayEffect> RestRecoveryEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	float RotateToBonfireTime = 0.5f;

	// ===== 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WaitDelayTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitExitRestEventTask = nullptr;

	// ===== 태그 =====
	FGameplayTag AbilityRestTag;
	FGameplayTag StateRestingTag;

	// ===== Bonfire 참조 =====
	TWeakObjectPtr<ABonfire> CachedBonfire = nullptr;

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

	// ===== 흐름 제어 =====
	void StartSitMontage();
	void TransitionToLoopMontage();
	void StartStandMontage();

	// ===== 헬퍼 =====
	void AcquireBonfire(const FGameplayEventData* TriggerEventData);
	void UpdateLastActivatedBonfire();
	void DisableCharacterMovement();
	void RestoreCharacterMovement();
	void ApplyRestRecovery();
	void RequestEnemyReset();

	UFUNCTION()
	void OnRotateDelayFinished();

	UFUNCTION()
	void OnExitRestEventReceived(FGameplayEventData Payload);

#pragma endregion

private:
#pragma region "Private Variables"

	enum class ERestState : uint8 { EnteringRest, RestLoop, LeavingRest };
	ERestState CurrentRestState = ERestState::EnteringRest;

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
