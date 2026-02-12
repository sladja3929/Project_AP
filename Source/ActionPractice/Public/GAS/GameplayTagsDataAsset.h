#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsDataAsset.generated.h"

// 태그를 추가하면 에디터의 데이터 에셋에서 꼭 실제 태그를 바인딩할 것
// 게임플레이 태그 서브시스템 클래스 위치: GAS/GameplayTagsSubsystem.h
UCLASS(BlueprintType)
class ACTIONPRACTICE_API UGameplayTagsDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Input Tags"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Tags")
	FGameplayTag Input_Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Tags")
	FGameplayTag Input_ChargeAttack;

#pragma endregion

#pragma region "Ability Tags"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Attack;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Attack_Normal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Attack_Charge;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Attack_Roll;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Attack_Sprint;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Attack_Jump;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Roll;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Sprint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Jump;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_Block;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Tags")
	FGameplayTag Ability_HitReaction;
#pragma endregion

#pragma region "State Tags"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_Attacking_Local;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_Attacking_Auth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_Blocking;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_Sprinting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_Jumping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_Rolling;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Ability_JustRolled;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Recovering_Auth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Recovering_Local;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Stunned;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_Invincible;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Tags")
	FGameplayTag State_StaminaRegenBlocked;
	
#pragma endregion

#pragma region "Event Tags"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_EnableBufferInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_ActionRecoveryStart;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_ActionRecoveryEnd;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_ResetCombo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_ChargeStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_InvincibleStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_HitDetectionStart;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_HitDetectionEnd;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_RotateToTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_CheckCondition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Notify_AddCombo;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Action_InputByBuffer;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Action_PlayBuffer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Action_AttackInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Event Tags")
	FGameplayTag Event_Action_CancelAttack;
	
#pragma endregion

#pragma region "Effect Tags"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_Invincibility_Duration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_JustRolled_Duration;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_Stamina_Cost;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_Stamina_RegenBlockDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_Sprint_SpeedMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_Damage_IncomingDamage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect Tags")
	FGameplayTag Effect_Cooldown_Duration;
#pragma endregion
};