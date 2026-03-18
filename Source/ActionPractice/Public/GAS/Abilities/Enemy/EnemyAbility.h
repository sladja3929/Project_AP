#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/BaseAbility.h"
#include "GameplayTagContainer.h"
#include "EnemyAbility.generated.h"

class UEnemyAbilitySystemComponent;
class UEnemyAttributeSet;
class AEnemyCharacter;
class ABossCharacter;
class AEnemyAIController;

UCLASS()
class ACTIONPRACTICE_API UEnemyAbility : public UBaseAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void ActivateInitSettings() override;

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

	UFUNCTION(BlueprintPure, Category = "Ability")
	UEnemyAbilitySystemComponent* GetEnemyAbilitySystemComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	AEnemyCharacter* GetEnemyCharacterFromActorInfo() const;

	//멤버변수 Actor Info 활성화 전 사용 (CanActivateAbility 등)
	AEnemyCharacter* GetEnemyCharacterFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	ABossCharacter* GetBossCharacterFromActorInfo() const;

	//멤버변수 Actor Info 활성화 전 사용 (CanActivateAbility 등)
	ABossCharacter* GetBossCharacterFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	UEnemyAttributeSet* GetEnemyAttributeSetFromActorInfo() const;

	//멤버변수 Actor Info 활성화 전 사용 (CanActivateAbility 등)
	UEnemyAttributeSet* GetEnemyAttributeSetFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	AEnemyAIController* GetEnemyAIControllerFromActorInfo() const;

	//멤버변수 Actor Info 활성화 전 사용 (CanActivateAbility 등)
	AEnemyAIController* GetEnemyAIControllerFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const;

#pragma endregion
};