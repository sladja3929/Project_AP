#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "BlockAbility.generated.h"

struct FBlockActionData;
class UAbilityTask_PlayMontageWithEvents;

UCLASS()
class ACTIONPRACTICE_API UBlockAbility : public UActionPracticeAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()
	
public:
#pragma region "Public Functions"
	UBlockAbility();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void ActivateInitSettings() override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	const FBlockActionData* WeaponBlockData = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;
	
#pragma endregion

#pragma region "Protected Functions"

	UFUNCTION()
	virtual UAnimMontage* SetMontageToPlayTask() override;
	
	virtual void SetUpPlayMontageWithEventsTask() override;

	UFUNCTION()
	virtual void StartMontageWithEventsTask() override;

	//Idle일 때는 호출되지 않음, 오로지 Reaction일때만
	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;
	
	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	virtual void OnWaitInputRelease(float TimeHeld) override;
	
#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};