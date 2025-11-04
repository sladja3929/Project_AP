#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/BaseAttackAbility.h"
#include "Engine/Engine.h"
#include "ChargeAttackAbility.generated.h"

UCLASS()
class ACTIONPRACTICE_API UChargeAttackAbility : public UBaseAttackAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions" //==================================================

	UChargeAttackAbility();
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

#pragma endregion

protected:
#pragma region "Protected Vriables" //================================================

	UPROPERTY()
	bool bMaxCharged = false;

	UPROPERTY()
	bool bIsCharging = false;

	UPROPERTY()
	bool bNoCharge = false;

	//PlayAction, ExecuteMontageTask 파라미터
	UPROPERTY()
	bool bCreateTask = false;

	UPROPERTY()
	bool bIsAttackMontage = false;

	//사용되는 태그들
	FGameplayTag EventNotifyResetComboTag;
	FGameplayTag EventNotifyChargeStartTag;

#pragma endregion

#pragma region "Protected Functions" //================================================

	virtual void ActivateInitSettings() override;
	virtual void SetHitDetectionConfig() override;
	virtual void SetStaminaCost(float InStaminaCost) override;
	virtual bool RotateCharacter() override;
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void ExecuteMontageTask() override;
	virtual void BindEventsAndReadyMontageTask() override;
	
	UFUNCTION()
	void PlayNextCharge();
	
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload) override;
	
	UFUNCTION()
	void OnNotifyResetCombo(FGameplayEventData Payload);

	UFUNCTION()
	void OnNotifyChargeStart(FGameplayEventData Payload);

	virtual void OnEventInputByBuffer(FGameplayEventData Payload) override;

	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;
	
#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};