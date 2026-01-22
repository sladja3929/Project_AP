#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/BaseAttackAbility.h"
#include "Engine/Engine.h"
#include "NormalAttackAbility.generated.h"

/*** 일반공격 트리거/액션 설명
 * 1. TryActivate로 어빌리티 실행
 * 2. PlayBuffer: ActionRecovery 사이에 입력이 들어옴. 연계 공격 수행
 * 3. InputPressed: ActionRecovery가 끝난 후 입력이 들어옴. 연계 공격 수행
 * 4. ResetCombo: 공격 몽타주 끝자락에 수신. 콤보 카운터 -1로 리셋
 * 5. 어빌리티 종료
 */

UCLASS()
class ACTIONPRACTICE_API UNormalAttackAbility : public UBaseAttackAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions" //==================================================

	UNormalAttackAbility();
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

#pragma endregion

protected:
#pragma region "Protected Vriables" //================================================

	//PlayAction, ExecuteMontageTask 파라미터
	UPROPERTY()
	bool bCreateTask = false;

	//사용되는 태그들
	FGameplayTag EventNotifyResetComboTag;
	
#pragma endregion

#pragma region "Protected Functions" //================================================

	virtual void ActivateInitSettings() override;
	virtual void ExecuteMontageTask() override;
	virtual void BindEventsAndReadyMontageTask() override;
	
	UFUNCTION()
	void PlayNextAttack();
	
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload) override;
	
	UFUNCTION()
	void OnNotifyResetCombo(FGameplayEventData Payload);
	
	virtual void OnEventInputByBuffer(FGameplayEventData Payload) override;
	
#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};