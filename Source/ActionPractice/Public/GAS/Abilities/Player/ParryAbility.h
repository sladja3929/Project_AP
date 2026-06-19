#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "ParryAbility.generated.h"

UCLASS()
class ACTIONPRACTICE_API UParryAbility : public UActionRecoveryAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UParryAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//커브 이름 상수
	static const FName CurveName_ParryWindow;

	//태그
	FGameplayTag StateParryingTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual bool ConsumeStamina() override;

	// ===== IMontageAbilityInterface =====
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;

	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskMontageInterrupted() override;

	// ===== 커브 에지 핸들러 =====
	virtual void OnCurveRisingEdgeReceived(FName CurveName) override;
	virtual void OnCurveFallingEdgeReceived(FName CurveName) override;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	//State.Parrying 태그 안전 제거
	void CleanupParryingTag();

	//패리 윈도우 진입: 소유 클라는 예측 윈도우로 적용, 서버는 권위+복제
	void AddParryingTag_Predicted();
	
#pragma endregion
};
