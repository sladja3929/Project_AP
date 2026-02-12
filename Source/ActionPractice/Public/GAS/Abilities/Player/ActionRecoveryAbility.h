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
 * 몽타주를 사용하며 액션동안 ActionRecovery가 존재하는 어빌리티.
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
		
	//회전이 락온을 무시할지 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotate")
	bool bIgnoreLockOn = false;

	//캐릭터 회전 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotate")
	float RotateTime = 0.1f;

	// ===== 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WaitDelayTask;
	
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInputByBufferEventTask;

	// ===== 게임플레이 태그 =====
	FGameplayTag ActionRecoveryStartTag;
	FGameplayTag ActionRecoveryEndTag;
	FGameplayTag EventInputByBufferTag;
	FGameplayTag EventPlayBufferTag;
	FGameplayTag EventEnableBufferInputTag;
	FGameplayTag StateRecoveringLocalTag;
	FGameplayTag StateRecoveringAuthTag;

	// ===== 커브 폴링 =====
	static const FName CurveName_EnableBufferInput;
	static const FName CurveName_ActionRecovery;

#pragma endregion

#pragma region "Protected Functions"

	virtual void ActivateInitSettings() override;

	UFUNCTION()
	virtual bool ConsumeStamina() PURE_VIRTUAL(UActionRecoveryAbility::ConsumeStamina, return false;);
	
	UFUNCTION()
	virtual bool RotateCharacter();

	//몽타주 실행 전 캐릭터 회전이 필요할 경우 StartMontageWithEventsTask 대신 실행하는 함수
	UFUNCTION()
	virtual void StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();

	// ===== 몽타주 인터페이스 함수 =====
	UFUNCTION()
	virtual UAnimMontage* SetMontageToPlayTask() override PURE_VIRTUAL(UMontageAbility::SetMontageToPlayTask, return nullptr; );

	UFUNCTION()
	virtual void StartMontageWithEventsTask() override;
	
	virtual void SetUpPlayMontageWithEventsTask() override;

	// ===== 핸들러 함수 =====
	UFUNCTION()
	virtual void OnTaskMontageCompleted() override PURE_VIRTUAL(UActionRecoveryAbility::OnTaskMontageCompleted, );

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override PURE_VIRTUAL(UActionRecoveryAbility::OnTaskMontageInterrupted, );

	//노티파이 이벤트를 전부 수신하는 콜백 함수, 실질적인 콜백 함수들의 중간다리 역할
	//bool 리턴값으로 Super 함수에서 이벤트 수신이 일어났는지 확인
	UFUNCTION()
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload);
	
	UFUNCTION()
	virtual void OnEventInputByBuffer(FGameplayEventData Payload) {}
	
	UFUNCTION()
	virtual void OnCurveRisingEdgeReceived(FName CurveName);

	UFUNCTION()
	virtual void OnCurveFallingEdgeReceived(FName CurveName);
	
	UFUNCTION()
	virtual void ProcessActionRecoveryEnd();

	//버퍼 관련 처리 (서버 권한 체크 포함)
	void ExecuteBuffer();

#pragma endregion

private:
#pragma region "Private Variables"

	//리커버리 종료 수행 여부 (EndAbility에서 체크용)
	bool bActionRecoveryEnded = false;
	
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};