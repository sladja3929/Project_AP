#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "ActionRecoveryAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UAbilityTask_WaitDelay;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

/***
 * 몽타주를 사용하며 ActionRecovery와 RotateCharacter가 있는 어빌리티.
 */
UCLASS(Abstract)
class ACTIONPRACTICE_API UActionRecoveryAbility : public UActionPracticeAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"
	UActionRecoveryAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
#pragma endregion

protected:
#pragma region "Protected Variables"

    //액션 수행 전 회전을 할지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotate")
	bool bRotateBeforeAction = true;
	
	//회전이 락온을 무시할지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotate")
	bool bIgnoreLockOn = false;

	//캐릭터 회전 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotate")
	float RotateTime = 0.1f;

	//PlayMontageWithEvents 태스크
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask;

	//딜레이 태스크
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WaitDelayTask;
	
	//이벤트 대기 태스크
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInputByBufferEventTask;

	//사용되는 태그들
	FGameplayTag ActionRecoveryStartTag;
	FGameplayTag ActionRecoveryEndTag;
	FGameplayTag EventInputByBufferTag;
	FGameplayTag EventPlayBufferTag;
	FGameplayTag StateRecoveringTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual void ActivateInitSettings() override;

	UFUNCTION()
	virtual void AddStateRecoveringTag();

	UFUNCTION()
	virtual void RemoveStateRecoveringTags();
	
	UFUNCTION()
	virtual bool ConsumeStamina();
	
	UFUNCTION()
	virtual bool RotateCharacter();
	
	UFUNCTION()
	virtual void PlayAction() override;

	UFUNCTION()
	virtual UAnimMontage* SetMontageToPlayTask() override PURE_VIRTUAL(UMontageAbility::SetMontageToPlayTask, return nullptr; );

	UFUNCTION()
	virtual void ExecuteMontageTask() override;
	
	virtual void BindEventsAndReadyMontageTask() override;

	UFUNCTION()
	virtual void ReadyInputByBufferTask();

	//몽타주 이벤트 콜백 함수들
	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	//노티파이 이벤트를 전부 수신하는 콜백 함수, 실질적인 콜백 함수들의 중간다리 역할
	//bool 리턴값으로 Super 함수에서 이벤트 수신이 일어났는지 확인
	UFUNCTION()
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload);

	//ActionRecovery 노티파이 콜백 함수들, State.Recovering는 노티파이와 별개로 스테미나 소모 체크 직후 무조건 부착
	UFUNCTION()
	virtual void OnEventActionRecoveryStart(FGameplayEventData Payload);
	
	UFUNCTION()
	virtual void OnEventActionRecoveryEnd(FGameplayEventData Payload);

	//InputByBuffer GameplayEvent 콜백 함수
	UFUNCTION()
	virtual void OnEventInputByBuffer(FGameplayEventData Payload) {}

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};