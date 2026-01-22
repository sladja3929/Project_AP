#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/BaseAttackAbility.h"
#include "Engine/Engine.h"
#include "ChargeAttackAbility.generated.h"

UENUM()
enum class EAPChargePhase : uint8
{
	Charging_PreStart,
	Charging_Active,
	Attack
};

/*** 차지공격 트리거/액션 설명
 * - Release: WaitInputReleaseTask에서 릴리즈 감지
 * - ChargeStart: 차지 몽타주 초기에 ChargeStart 수신
 * - ChargeComplete: MaxCharge, 차지 몽타주가 끝날 때 수신
 * - ActionRecovery: 차지 몽타주 시작할 때 Start, 공격 몽타주 도중에 End 수신
 * - PlayBuffer: ActionRecovery 사이에 입력이 들어와서 ActionRecoveryEnd 때 실행
 * - InputPressed: ActionRecovery가 끝난 후 입력이 들어옴
 * - ResetCombo: 공격 몽타주 끝자락에 수신
 * 
 * 1. TryActivate로 어빌리티 실행, 즉시 WaitInputReleaseTask 시작. 차지공격은 차지 몽타주와 공격 몽타주로 나뉨
 * 2. Release < ChargeStart: ChargeStart 전에 릴리즈. 즉시 공격 몽타주로 전환, ReleaseTask 정리
 * 3. ChargeStart < Release < ChargeComplete: 차지 도중 릴리즈. 즉시 공격 몽타주로 전환
 * 4. ChargeComplete < Release: 차지 몽타주를 모두 완료. MaxCharge로 즉시 공격 몽타주로 전환
 * 5. PlayBuffer: 연계 공격 수행 = 다음 차지 몽타주 수행
 * 6. InputPressed: 연계 공격 수행 = 다음 차지 몽타주 수행
 * 7. ResetCombo: 공격 콤보 초기화. 콤보 카운터 -1로 리셋
 * 8. 어빌리티 종료
 */

UCLASS()
class ACTIONPRACTICE_API UChargeAttackAbility : public UBaseAttackAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions" //==================================================

	UChargeAttackAbility();
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

#pragma endregion

protected:
#pragma region "Protected Vriables" //================================================

	UPROPERTY()
	bool bMaxCharged = false;

	UPROPERTY()
	bool bIsChargingMontage = false;

	UPROPERTY()
	bool bNoCharge = false;

	//PlayAction, ExecuteMontageTask 파라미터
	UPROPERTY()
	bool bCreateTask = false;

	UPROPERTY()
	bool bIsAttackMontage = false;

	//사용되는 태그들
	FGameplayTag EventNotifyResetComboTag;
	FGameplayTag EventNotifyChargeStartTag;

	//커브 폴링 관련
	static const FName CurveName_ChargeStart;
	
#pragma endregion

#pragma region "Protected Functions" //================================================

	virtual void ActivateInitSettings() override;
	virtual void SetHitDetectionConfig() override;
	virtual void SetStaminaCost(float InStaminaCost) override;
	virtual bool RotateCharacter() override;
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void ExecuteMontageTask() override;
	virtual void BindEventsAndReadyMontageTask() override;
	
	UFUNCTION()
	void PlayNextCharge();
	
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload) override;
	
	UFUNCTION()
	void OnNotifyResetCombo(FGameplayEventData Payload);

	virtual void OnEventInputByBuffer(FGameplayEventData Payload) override;

	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	virtual void OnCurveRisingEdgeReceived(FName CurveName) override;

	virtual void HandleWaitInputReleased(float TimeHeld) override;
	
#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion	
};