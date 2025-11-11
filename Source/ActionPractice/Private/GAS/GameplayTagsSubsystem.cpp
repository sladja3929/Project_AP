#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/GameplayTagsDataAsset.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void UGameplayTagsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// 데이터 에셋 로드 (에디터에서 설정한 데이터 에셋 경로)
	// TODO: 프로젝트 설정 또는 기본 경로에서 데이터 에셋 로드
	const FString DataAssetPath = TEXT("/Game/GAS/DA_GameplayTags");
	GameplayTagsDataAsset = LoadObject<UGameplayTagsDataAsset>(nullptr, *DataAssetPath);
	
	if (!GameplayTagsDataAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameplayTagsDataAsset could not be loaded from path: %s"), *DataAssetPath);
	}
}

UGameplayTagsSubsystem* UGameplayTagsSubsystem::Get()
{
	if (GEngine)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.World() && WorldContext.WorldType != EWorldType::EditorPreview)
			{
				if (UGameInstance* GameInstance = WorldContext.World()->GetGameInstance())
				{
					return GameInstance->GetSubsystem<UGameplayTagsSubsystem>();
				}
			}
		}
	}
	
	return nullptr;
}

#pragma region "Static Ability Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityAttackTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackNormalTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityAttackNormalTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackChargeTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityAttackChargeTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackRollTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityAttackRollTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackSprintTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityAttackSprintTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackJumpTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityAttackJumpTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityRollTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityRollTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilitySprintTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilitySprintTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityJumpTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityJumpTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityBlockTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityBlockTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityHitReactionTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetAbilityHitReactionTagInternal();
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Static State Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityAttackingTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateAbilityAttackingTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityBlockingTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateAbilityBlockingTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityJumpingTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateAbilityJumpingTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilitySprintingTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateAbilitySprintingTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityRollingTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateAbilityRollingTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityJustRolledTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateAbilityJustRolledTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateRecoveringTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateRecoveringTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateStunnedTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateStunnedTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateInvincibleTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateInvincibleTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateStaminaRegenBlockedTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetStateStaminaRegenBlockedTagInternal();
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Static Event Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyEnableBufferInputTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyEnableBufferInputTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyActionRecoveryStartTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyActionRecoveryStartTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyActionRecoveryEndTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyActionRecoveryEndTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyResetComboTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyResetComboTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyChargeStartTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyChargeStartTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyInvincibleStartTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyInvincibleStartTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyHitDetectionStartTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyHitDetectionEndTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyRotateToTargetTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyAddComboTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventNotifyAddComboTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventActionInputByBufferTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventActionInputByBufferTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventActionPlayBufferTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEventActionPlayBufferTagInternal();
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Static Effect Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetEffectInvincibilityDurationTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEffectInvincibilityDurationTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectJustRolledDurationTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEffectJustRolledDurationTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectStaminaCostTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEffectStaminaCostTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectStaminaRegenBlockDurationTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEffectStaminaRegenBlockDurationTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectSprintSpeedMultiplierTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEffectSprintSpeedMultiplierTagInternal();
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectDamageIncomingDamageTag()
{
	if (UGameplayTagsSubsystem* Subsystem = Get())
	{
		return Subsystem->GetEffectDamageIncomingDamageTagInternal();
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Internal Ability Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Attack;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackNormalTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Attack_Normal;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackChargeTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Attack_Charge;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackRollTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Attack_Roll;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackSprintTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Attack_Sprint;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityAttackJumpTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Attack_Jump;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityRollTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Roll;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilitySprintTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Sprint;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityJumpTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Jump;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityBlockTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_Block;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetAbilityHitReactionTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Ability_HitReaction;
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Internal State Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityAttackingTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Ability_Attacking;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityBlockingTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Ability_Blocking;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityJumpingTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Ability_Jumping;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilitySprintingTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Ability_Sprinting;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityRollingTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Ability_Rolling;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateAbilityJustRolledTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Ability_JustRolled;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateRecoveringTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Recovering;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateStunnedTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Stunned;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateInvincibleTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_Invincible;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetStateStaminaRegenBlockedTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->State_StaminaRegenBlocked;
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Internal Event Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyEnableBufferInputTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_EnableBufferInput;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyActionRecoveryStartTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_ActionRecoveryStart;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyActionRecoveryEndTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_ActionRecoveryEnd;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyResetComboTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_ResetCombo;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyChargeStartTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_ChargeStart;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyInvincibleStartTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_InvincibleStart;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_HitDetectionStart;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_HitDetectionEnd;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_RotateToTarget;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventNotifyAddComboTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Notify_AddCombo;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventActionInputByBufferTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Action_InputByBuffer;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEventActionPlayBufferTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Event_Action_PlayBuffer;
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion

#pragma region "Internal Effect Tags"
const FGameplayTag& UGameplayTagsSubsystem::GetEffectInvincibilityDurationTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Effect_Invincibility_Duration;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectJustRolledDurationTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Effect_JustRolled_Duration;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectStaminaCostTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Effect_Stamina_Cost;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectStaminaRegenBlockDurationTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Effect_Stamina_RegenBlockDuration;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectSprintSpeedMultiplierTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Effect_Sprint_SpeedMultiplier;
	}
	return FGameplayTag::EmptyTag;
}

const FGameplayTag& UGameplayTagsSubsystem::GetEffectDamageIncomingDamageTagInternal() const
{
	if (GameplayTagsDataAsset)
	{
		return GameplayTagsDataAsset->Effect_Damage_IncomingDamage;
	}
	return FGameplayTag::EmptyTag;
}
#pragma endregion
