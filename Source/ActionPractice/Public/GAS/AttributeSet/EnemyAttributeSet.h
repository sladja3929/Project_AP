#pragma once

#include "CoreMinimal.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class ACTIONPRACTICE_API UEnemyAttributeSet : public UBaseAttributeSet
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	// ===== Base Attributes =====

	// ===== Stance (Groggy Gauge) =====

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_Stance)
	FGameplayAttributeData Stance;
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, Stance)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_MaxStance)
	FGameplayAttributeData MaxStance;
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MaxStance)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_StanceRegenRate)
	FGameplayAttributeData StanceRegenRate;
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, StanceRegenRate)

	// ===== Calculated Attributes =====
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_PhysicalAttackPower)
	FGameplayAttributeData PhysicalAttackPower;
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, PhysicalAttackPower)

	// ===== Meta Attributes =====

#pragma endregion

#pragma region "Public Functions"
	UEnemyAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//Duration, Infinite에서 수행
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	//Instant, Periodic에서 수행
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	//GE 직후 Instant, Periodic에서 수행
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

#pragma endregion

protected:
#pragma region "Protected Functions"

	UFUNCTION()
	virtual void OnRep_Stance(const FGameplayAttributeData& OldStance);

	UFUNCTION()
	virtual void OnRep_MaxStance(const FGameplayAttributeData& OldMaxStance);

	UFUNCTION()
	virtual void OnRep_StanceRegenRate(const FGameplayAttributeData& OldStanceRegenRate);

	UFUNCTION()
	virtual void OnRep_PhysicalAttackPower(const FGameplayAttributeData& OldPhysicalAttackPower);

#pragma endregion
};
