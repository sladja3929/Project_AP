#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "RollAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;

UCLASS()
class ACTIONPRACTICE_API URollAbility : public UActionRecoveryAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"
	URollAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roll")
	TObjectPtr<UAnimMontage> RollMontage = nullptr;

	//무적 상태 Gameplay Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roll")
	TSubclassOf<UGameplayEffect> InvincibilityEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roll")
	float InvincibilityDuration = 0.5f;

	//JustRolled 태그 부여용 Effect
	UPROPERTY(EditDefaultsOnly, Category="Roll")
	TSubclassOf<UGameplayEffect> JustRolledWindowEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roll")
	float JustRolledWindowDuration = 0.1f;

	//이벤트 대기 태스크
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInvincibleStartEventTask;
	
	//사용되는 태그들
	FGameplayTag EventNotifyInvincibleStartTag;
	FGameplayTag EffectInvincibilityDurationTag;
	FGameplayTag EffectJustRolledDurationTag;
	
#pragma endregion

#pragma region "Protected Functions"

	virtual bool ConsumeStamina() override;

	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;

	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskMontageInterrupted() override;

	//커브 기반 ActionRecovery 종료 처리
	virtual void ProcessActionRecoveryEnd() override;

	UFUNCTION()
	virtual void OnEventInvincibleStart(FGameplayEventData Payload);
	
	UFUNCTION(BlueprintCallable, Category = "Roll")
	virtual void ApplyInvincibilityEffect();
	
#pragma endregion

private:
#pragma region "Private Variables"

	FActiveGameplayEffectHandle InvincibilityEffectHandle;
	
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};