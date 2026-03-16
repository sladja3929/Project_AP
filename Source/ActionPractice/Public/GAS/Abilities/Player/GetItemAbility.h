#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "GetItemAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class APickupItem;
class UItemManagerComponent;

UCLASS()
class ACTIONPRACTICE_API UGetItemAbility : public UActionPracticeAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UGetItemAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//픽업 연출 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GetItem")
	TObjectPtr<UAnimMontage> PickupMontage = nullptr;

	// ===== 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	// ===== 캐싱 =====
	TWeakObjectPtr<APickupItem> CachedPickupItem = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	// ===== IMontageAbilityInterface =====
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;
	virtual void StartMontageWithEventsTask() override;

	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	// ===== 헬퍼 =====
	//TriggerEventData에서 PickupItem 참조 획득
	void AcquirePickupItem(const FGameplayEventData* TriggerEventData);

	//아이템을 인벤토리에 즉시 추가
	bool ProcessItemAcquisition();

	//무기 표시/숨김 — RestAbility 패턴과 동일
	void SetWeaponsVisibility(bool bVisible);

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
