#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "EnemyDeathAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UEnemyDataAsset;

UCLASS()
class ACTIONPRACTICE_API UEnemyDeathAbility : public UEnemyAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UEnemyDeathAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	//태그
	FGameplayTag StateDeadTag;

	//캐시된 사망 몽타주 (EnemyDataAsset에서 가져옴)
	UPROPERTY()
	TObjectPtr<UAnimMontage> CachedDeathMontage = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	// ===== 초기화 =====
	virtual void ActivateInitSettings() override;

	//서버 전용 사망 처리 — 자식에서 override하여 추가 로직 수행
	virtual void ExecuteDeathServerLogic();

	// ===== IMontageAbilityInterface =====
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;
	virtual void StartMontageWithEventsTask() override;

	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	UFUNCTION()
	virtual void OnTaskMontageBlendOut();

	// ===== 헬퍼 =====
	void AddStateDeadTag();
	void DisableCharacterMovement();
	void StopEnemyAI();
	void FreezeAnimPose();
	void HideHealthBar();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
