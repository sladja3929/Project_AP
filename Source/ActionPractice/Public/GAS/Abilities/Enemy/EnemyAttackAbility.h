#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "GAS/Abilities/HitDetectionSetter.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "AI/EnemyAIController.h"
#include "EnemyAttackAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
struct FFinalAttackData;
struct FEnemyTaggedAttackData;

/***
 *
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyAttackAbility : public UEnemyAbility, public IHitDetectionUser, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"


#pragma endregion

#pragma region "Public Functions"

	UEnemyAttackAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	// ===== 상태 관리 =====
	UPROPERTY()
	int32 ComboCounter = 0;

	UPROPERTY()
	int32 MaxComboCount = 0;

	//다음 콤보를 이어갈지 여부 체크
	bool bPerformNextCombo = true;

	// ===== 공격 데이터 =====
	const FEnemyTaggedAttackData* EnemyAttackData = nullptr;

	//Ability 시작 시 캐싱된 Target 정보
	FCurrentTarget CachedTargetInfo;

	// ===== 히트 디텍션 =====
	UPROPERTY()
	FHitDetectionSetter HitDetectionSetter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> DamageInstantEffect;

	// ===== 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitRotateToTargetEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitCheckConditionEventTask = nullptr;

	// ===== 게임플레이 태그 =====
	FGameplayTag EventNotifyRotateToTargetTag;
	FGameplayTag EventNotifyCheckConditionTag;

	// ===== 커브 이름 =====
	static const FName CurveName_ActionRecovery;

#pragma endregion

#pragma region "Protected Functions"

	// ===== 설정 / 초기화 =====
	virtual void ActivateInitSettings() override;
	void CacheGameplayTags();
	void CacheEnemyData();
	void BindHitDetectionSetter();

	// ===== 실행 로직 =====
	void ExecuteAttack();

	// ===== 히트 디텍션 (IHitDetectionUser) =====
	virtual void SetHitDetectionConfig() override;
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	// ===== 몽타주 태스크 (IMontageAbilityInterface) =====
	UFUNCTION()
	virtual UAnimMontage* SetMontageToPlayTask() override;

	UFUNCTION()
	virtual void StartMontageWithEventsTask() override;

	virtual void SetUpPlayMontageWithEventsTask() override;

	// ===== 핸들러 함수 =====
	//몽타주 핸들러
	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	//이벤트 핸들러
	UFUNCTION()
	virtual void OnEventRotateToTarget(FGameplayEventData Payload);

	UFUNCTION()
	virtual void OnEventCheckCondition(FGameplayEventData Payload);

	//커브 에지 핸들러
	UFUNCTION()
	virtual void OnCurveRisingEdgeReceived(FName CurveName);

	UFUNCTION()
	virtual void OnCurveFallingEdgeReceived(FName CurveName);

	virtual void OnActionRecoveryEnd();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};