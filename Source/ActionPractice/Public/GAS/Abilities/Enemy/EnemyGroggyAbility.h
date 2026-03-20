#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "EnemyGroggyAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UEnemyDataAsset;

//그로기 진행 단계
UENUM()
enum class EGroggyPhase : uint8
{
	None,
	Start,
	Loop,
	End
};

UCLASS()
class ACTIONPRACTICE_API UEnemyGroggyAbility : public UEnemyAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UEnemyGroggyAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	//EnemyDataAsset에서 캐시한 몽타주
	UPROPERTY()
	TObjectPtr<UAnimMontage> CachedGroggyStartMontage = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> CachedGroggyLoopMontage = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> CachedGroggyEndMontage = nullptr;

	//그로기 루프 지속 시간
	float CachedGroggyLoopDuration = 3.0f;

	//현재 그로기 단계
	EGroggyPhase CurrentPhase = EGroggyPhase::None;

	//루프 타이머 핸들
	FTimerHandle GroggyLoopTimerHandle;

	//태그
	FGameplayTag StateGroggyTag;

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

	// ===== Phase 전환 =====
	void StartGroggyLoop();
	void OnGroggyLoopTimerExpired();
	void StartGroggyEnd();

	// ===== Stamina 복구 =====
	void ResetStaminaToMax();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
