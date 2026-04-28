#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "WeaponSwitchAbility.generated.h"

class UWeaponManagerComponent;
class AWeapon;

UCLASS()
class ACTIONPRACTICE_API UWeaponSwitchAbility : public UActionRecoveryAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UWeaponSwitchAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//교체 몽타주 (BP에서 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Switch")
	TObjectPtr<UAnimMontage> WeaponSwitchMontage = nullptr;

	//현재 활성화에서 사용하는 좌/우 구분 (TriggerEventData에서 결정)
	bool bIsLeftHand = false;

	//좌/우 판별용 태그 (OnGiveAbility에서 캐싱)
	FGameplayTag InputCycleRightWeaponTag;
	FGameplayTag InputCycleLeftWeaponTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual bool ConsumeStamina() override;
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskMontageInterrupted() override;

	//해당 손의 무기 표시/숨김
	void SetSwitchingWeaponVisibility(bool bVisible);

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
