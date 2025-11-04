#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "GAS/Abilities/HitDetectionSetter.h"
#include "Engine/Engine.h"
#include "BaseAttackAbility.generated.h"

struct FFinalAttackData;
class UAbilityTask_PlayMontageWithEvents;
class UAbilityTask_WaitGameplayEvent;

struct FTaggedAttackData;
UCLASS()
class ACTIONPRACTICE_API UBaseAttackAbility : public UActionRecoveryAbility, public IHitDetectionUser
{
	GENERATED_BODY()

public:
#pragma region "Public Functions" //==================================================

	UBaseAttackAbility();
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Vriables" //================================================

	const FTaggedAttackData* WeaponAttackData = nullptr;
	
	int32 ComboCounter = 0;
	int32 MaxComboCount = 0;

	//HitDetection 관련
	UPROPERTY()
	FHitDetectionSetter HitDetectionSetter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> DamageInstantEffect;

#pragma endregion

#pragma region "Protected Functions" //================================================

	//IHitDetectionUser 인터페이스 구현
	virtual void SetHitDetectionConfig() override;
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	virtual void ActivateInitSettings() override;
	virtual bool ConsumeStamina() override;
	virtual void PlayAction() override;
	virtual UAnimMontage* SetMontageToPlayTask() override;

#pragma endregion

private:
#pragma region "Private Variables"


#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};