#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "SprintAbility.generated.h"

UCLASS()
class ACTIONPRACTICE_API USprintAbility : public UActionPracticeAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
	
#pragma endregion

#pragma region "Public Functions"
	USprintAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	#pragma endregion

protected:
#pragma region "Protected Variables"
	
	//캐릭터에서 참조
	UPROPERTY()
	float SprintSpeedMultiplier = 1.5f;

	//로컬 입력 기반 종료 임계값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Config")
	float MovementInputThreshold = 0.1f;

	//서버에서 이동 판정용 임계값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Config")
	float ServerMinAccelerationToKeepSprint = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint|Config")
	float ServerMinSpeedToKeepSprint = 10.0f;
	
	UPROPERTY(EditDefaultsOnly, Category="Cost")
	TSubclassOf<class UGameplayEffect> StaminaDrainEffect;

	FActiveGameplayEffectHandle StaminaDrainHandle;

	UPROPERTY(EditDefaultsOnly, Category="Cost")
	TSubclassOf<class UGameplayEffect> SprintEffect;
    
	FActiveGameplayEffectHandle SprintHandle;
	FGameplayTag EffectSprintSpeedMultiplierTag;

	FTimerHandle SprintCheckTimer;
	
#pragma endregion
	
#pragma region "Protected Functions"
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
	
	void CheckSprintConditions();
	//
	bool ShouldRunSprintChecks() const;
	
#pragma endregion

private:
#pragma region "Private Variables"
	
	float OriginalMaxWalkSpeed = 0.0f;

#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};