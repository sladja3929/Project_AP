#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsSubsystem.generated.h"

class UGameplayTagsDataAsset;

UCLASS()
class ACTIONPRACTICE_API UGameplayTagsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma region "Static Ability Tags"
	static const FGameplayTag& GetAbilityAttackTag();
	static const FGameplayTag& GetAbilityAttackNormalTag();
	static const FGameplayTag& GetAbilityAttackChargeTag();
	static const FGameplayTag& GetAbilityAttackRollTag();
	static const FGameplayTag& GetAbilityAttackSprintTag();
	static const FGameplayTag& GetAbilityAttackJumpTag();
	static const FGameplayTag& GetAbilityRollTag();
	static const FGameplayTag& GetAbilitySprintTag();
	static const FGameplayTag& GetAbilityJumpTag();
	static const FGameplayTag& GetAbilityBlockTag();
	static const FGameplayTag& GetAbilityHitReactionTag();
#pragma endregion

#pragma region "Static State Tags"
	static const FGameplayTag& GetStateAbilityAttackingTag();
	static const FGameplayTag& GetStateAbilityBlockingTag();
	static const FGameplayTag& GetStateAbilityJumpingTag();
	static const FGameplayTag& GetStateAbilitySprintingTag();
	static const FGameplayTag& GetStateAbilityRollingTag();
	static const FGameplayTag& GetStateAbilityJustRolledTag();
	static const FGameplayTag& GetStateRecoveringTag();
	static const FGameplayTag& GetStateStunnedTag();
	static const FGameplayTag& GetStateInvincibleTag();
	static const FGameplayTag& GetStateStaminaRegenBlockedTag();
#pragma endregion

#pragma region "Static Event Tags"
	static const FGameplayTag& GetEventNotifyEnableBufferInputTag();
	static const FGameplayTag& GetEventNotifyActionRecoveryStartTag();
	static const FGameplayTag& GetEventNotifyActionRecoveryEndTag();
	static const FGameplayTag& GetEventNotifyResetComboTag();
	static const FGameplayTag& GetEventNotifyChargeStartTag();
	static const FGameplayTag& GetEventNotifyInvincibleStartTag();
	static const FGameplayTag& GetEventNotifyHitDetectionStartTag();
	static const FGameplayTag& GetEventNotifyHitDetectionEndTag();
	static const FGameplayTag& GetEventNotifyRotateToTargetTag();
	static const FGameplayTag& GetEventNotifyCheckConditionTag();
	static const FGameplayTag& GetEventNotifyAddComboTag();
	static const FGameplayTag& GetEventActionInputByBufferTag();
	static const FGameplayTag& GetEventActionPlayBufferTag();
#pragma endregion

#pragma region "Static Effect Tags"
	static const FGameplayTag& GetEffectInvincibilityDurationTag();
	static const FGameplayTag& GetEffectJustRolledDurationTag();
	static const FGameplayTag& GetEffectStaminaCostTag();
	static const FGameplayTag& GetEffectStaminaRegenBlockDurationTag();
	static const FGameplayTag& GetEffectSprintSpeedMultiplierTag();
	static const FGameplayTag& GetEffectDamageIncomingDamageTag();
#pragma endregion

private:
	// Internal helper function
	static UGameplayTagsSubsystem* Get();

#pragma region "Internal Ability Tags"
	const FGameplayTag& GetAbilityAttackTagInternal() const;
	const FGameplayTag& GetAbilityAttackNormalTagInternal() const;
	const FGameplayTag& GetAbilityAttackChargeTagInternal() const;
	const FGameplayTag& GetAbilityAttackRollTagInternal() const;
	const FGameplayTag& GetAbilityAttackSprintTagInternal() const;
	const FGameplayTag& GetAbilityAttackJumpTagInternal() const;
	const FGameplayTag& GetAbilityRollTagInternal() const;
	const FGameplayTag& GetAbilitySprintTagInternal() const;
	const FGameplayTag& GetAbilityJumpTagInternal() const;
	const FGameplayTag& GetAbilityBlockTagInternal() const;
	const FGameplayTag& GetAbilityHitReactionTagInternal() const;
#pragma endregion

#pragma region "Internal State Tags"
	const FGameplayTag& GetStateAbilityAttackingTagInternal() const;
	const FGameplayTag& GetStateAbilityBlockingTagInternal() const;
	const FGameplayTag& GetStateAbilityJumpingTagInternal() const;
	const FGameplayTag& GetStateAbilitySprintingTagInternal() const;
	const FGameplayTag& GetStateAbilityRollingTagInternal() const;
	const FGameplayTag& GetStateAbilityJustRolledTagInternal() const;
	const FGameplayTag& GetStateRecoveringTagInternal() const;
	const FGameplayTag& GetStateStunnedTagInternal() const;
	const FGameplayTag& GetStateInvincibleTagInternal() const;
	const FGameplayTag& GetStateStaminaRegenBlockedTagInternal() const;
#pragma endregion

#pragma region "Internal Event Tags"
	const FGameplayTag& GetEventNotifyEnableBufferInputTagInternal() const;
	const FGameplayTag& GetEventNotifyActionRecoveryStartTagInternal() const;
	const FGameplayTag& GetEventNotifyActionRecoveryEndTagInternal() const;
	const FGameplayTag& GetEventNotifyResetComboTagInternal() const;
	const FGameplayTag& GetEventNotifyChargeStartTagInternal() const;
	const FGameplayTag& GetEventNotifyInvincibleStartTagInternal() const;
	const FGameplayTag& GetEventNotifyHitDetectionStartTagInternal() const;
	const FGameplayTag& GetEventNotifyHitDetectionEndTagInternal() const;
	const FGameplayTag& GetEventNotifyRotateToTargetTagInternal() const;
	const FGameplayTag& GetEventNotifyCheckConditionTagInternal() const;
	const FGameplayTag& GetEventNotifyAddComboTagInternal() const;
	const FGameplayTag& GetEventActionInputByBufferTagInternal() const;
	const FGameplayTag& GetEventActionPlayBufferTagInternal() const;
#pragma endregion

#pragma region "Internal Effect Tags"
	const FGameplayTag& GetEffectInvincibilityDurationTagInternal() const;
	const FGameplayTag& GetEffectJustRolledDurationTagInternal() const;
	const FGameplayTag& GetEffectStaminaCostTagInternal() const;
	const FGameplayTag& GetEffectStaminaRegenBlockDurationTagInternal() const;
	const FGameplayTag& GetEffectSprintSpeedMultiplierTagInternal() const;
	const FGameplayTag& GetEffectDamageIncomingDamageTagInternal() const;
#pragma endregion
	
protected:
	// 태그 데이터 에셋
	UPROPERTY()
	TObjectPtr<UGameplayTagsDataAsset> GameplayTagsDataAsset;
};