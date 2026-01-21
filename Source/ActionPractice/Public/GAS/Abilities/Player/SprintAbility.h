#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "SprintAbility.generated.h"

UCLASS()
class ACTIONPRACTICE_API USprintAbility : public UActionPracticeAbility
{
	GENERATED_BODY()

public:
	USprintAbility();

protected:
	
	//캐릭터에서 참조
	UPROPERTY()
	float SprintSpeedMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Cost")
	TSubclassOf<class UGameplayEffect> StaminaDrainEffect;

	FActiveGameplayEffectHandle StaminaDrainHandle;

	UPROPERTY(EditDefaultsOnly, Category="Cost")
	TSubclassOf<class UGameplayEffect> SprintEffect;
    
	FActiveGameplayEffectHandle SprintHandle;
	FGameplayTag EffectSprintSpeedMultiplierTag;

public:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void ActivateInitSettings() override;
	
	UFUNCTION(BlueprintCallable, Category = "Sprint")
	virtual void StartSprinting();

	UFUNCTION(BlueprintCallable, Category = "Sprint")
	virtual void StopSprinting();

	virtual void HandleSprinting();

	bool StartSprintEffect();
	void StopSprintEffect();
	
	virtual bool CanContinueSprinting() const;

	virtual bool StartStaminaDrainEffect();
	virtual void StopStaminaDrainEffect();

	virtual void HandleWaitInputReleased(float TimeHeld) override;

private:
	FTimerHandle SprintCheckTimer;

	float OriginalMaxWalkSpeed = 0.0f;

	void CheckSprintConditions();
};