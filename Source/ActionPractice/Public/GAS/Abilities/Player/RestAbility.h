#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "RestAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitDelay;
class ABonfire;
class APlayerCameraManager;
class UMaterialInterface;

UCLASS()
class ACTIONPRACTICE_API URestAbility : public UActionPracticeAbility, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	URestAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TObjectPtr<UAnimMontage> SitMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TObjectPtr<UAnimMontage> LoopMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TObjectPtr<UAnimMontage> StandMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	TSubclassOf<UGameplayEffect> RestRecoveryEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest")
	float RotateToBonfireTime = 0.5f;

	// ===== 리셋 화면 가림 (카메라 모디파이어 + 포스트 프로세스 머티리얼) =====
	//모닥불 주변 빛 웅덩이만 남기고 나머지를 어둠으로 덮는 PP 머티리얼 (스칼라 Intensity 0~1, 벡터 FocusLocation)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest|Fade")
	TObjectPtr<UMaterialInterface> RestScreenMaskMaterial = nullptr;

	//화면이 완전히 덮일 때까지 시간, 끝나는 시점에 회복·적 리셋 실행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest|Fade", meta = (ClampMin = "0.0"))
	float RestFadeOutTime = 0.6f;

	//리셋 후 일어서기 입력을 받기 전 최소 유지 시간 (데디 서버 적 리스폰 복제 지연 흡수)
	//클라는 리셋 완료 신호 없이 시간으로만 판단하므로 지터·고핑 완충은 이 값이 유일함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest|Fade", meta = (ClampMin = "0.0"))
	float RestFadeHoldTime = 0.5f;

	//일어설 때 화면이 다시 밝아지는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rest|Fade", meta = (ClampMin = "0.0"))
	float RestFadeInTime = 0.8f;

	// ===== 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WaitDelayTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> RestFadeDelayTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitExitRestEventTask = nullptr;

	// ===== 태그 =====
	FGameplayTag AbilityRestTag;
	FGameplayTag StateRestingTag;

	// ===== Bonfire 참조 =====
	TWeakObjectPtr<ABonfire> CachedBonfire = nullptr;

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

	UFUNCTION()
	void OnTaskMontageBlendOut();

	// ===== 흐름 제어 =====
	void StartSitMontage();
	void TransitionToLoopMontage();
	void StartStandMontage();

	// ===== 헬퍼 =====
	void SetWeaponsVisibility(bool bVisible);
	void AcquireBonfire(const FGameplayEventData* TriggerEventData);
	void UpdateLastActivatedBonfire();
	void DisableCharacterMovement();
	void RestoreCharacterMovement();
	void ApplyRestRecovery();
	void RequestEnemyReset();

	UFUNCTION()
	void OnRotateDelayFinished();

	UFUNCTION()
	void OnExitRestEventReceived(FGameplayEventData Payload);

	// ===== 리셋 화면 가림 =====
	UFUNCTION()
	void OnRestFadeOutFinished();

	UFUNCTION()
	void OnRestFadeHoldFinished();

#pragma endregion

private:
#pragma region "Private Variables"

	enum class ERestState : uint8 { EnteringRest, RestLoop, LeavingRest };
	ERestState CurrentRestState = ERestState::EnteringRest;

	//로컬 화면을 덮은 뒤 아직 밝히지 않은 상태 (중도 종료 시 복구 판단)
	bool bRestFadeCovering = false;

#pragma endregion

#pragma region "Private Functions"

	APlayerCameraManager* GetLocalCameraManager() const;

	//로컬 화면 가림 시작(bCover=true) 또는 해제, 현재 강도에서 Duration 동안 진행
	void SetLocalRestScreenMask(bool bCover, float Duration);

#pragma endregion
};
