#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/BaseAttackAbility.h"
#include "Engine/Engine.h"
#include "ChargeAttackAbility.generated.h"

UENUM()
enum class EAPChargePhase : uint8
{
	Charging_PreStart,
	Charging_Active,
	Attack
};

UCLASS()
class ACTIONPRACTICE_API UChargeAttackAbility : public UBaseAttackAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions" //==================================================

	UChargeAttackAbility();
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

#pragma endregion

protected:
#pragma region "Protected Vriables" //================================================

	UPROPERTY()
	bool bMaxCharged = false;

	//PlayAction, ExecuteMontageTask 파라미터
	UPROPERTY()
	bool bCreateTask = false;

	UPROPERTY()
	bool bIsAttackMontage = false;

	//사용되는 태그들
	FGameplayTag EventNotifyResetComboTag;

	//커브 폴링 관련
	static const FName CurveName_ChargeStart;

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
	void PlayNextCharge(bool bInReleaseRequested);
	
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload) override;
	
	UFUNCTION()
	void OnNotifyResetCombo(FGameplayEventData Payload);

	virtual void OnCurveRisingEdgeReceived(FName CurveName) override;

	virtual void OnEventInputByBuffer(FGameplayEventData Payload) override;

	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	virtual void HandleWaitInputReleased(float TimeHeld) override;

#pragma endregion

private:
#pragma region "Private Variables"

	EAPChargePhase ChargePhase = EAPChargePhase::Charging_PreStart;

	bool bReleaseRequested = false;

#pragma endregion

#pragma region "Private Functions"

	void TransitionToAttack(bool bInMaxCharged, bool bInCreateNewTask);

#pragma endregion
};