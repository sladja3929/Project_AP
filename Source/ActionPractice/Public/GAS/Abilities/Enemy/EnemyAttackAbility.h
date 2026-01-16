#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "GAS/Abilities/HitDetectionSetter.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "AI/EnemyAIController.h"
#include "EnemyAttackAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
struct FFinalAttackData;
struct FNamedAttackData;

/***
 *
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyAttackAbility : public UEnemyAbility, public IHitDetectionUser, public IMontageAbilityInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	FName AttackName = NAME_None;

#pragma endregion

#pragma region "Public Functions"

	UEnemyAttackAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//PlayMontageWithEvents 태스크
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageWithEvents> PlayMontageWithEventsTask;

	//HitDetection 관련
	UPROPERTY()
	FHitDetectionSetter HitDetectionSetter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> DamageInstantEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	float RotateTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	float MaxTargetDistance = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	float MaxTargetAngle = 60.0f;

	//공격 데이터
	const FNamedAttackData* EnemyAttackData = nullptr;

	//콤보 카운터
	int32 ComboCounter = 0;
	int32 MaxComboCount = 0;

	//다음 콤보를 이어갈지 여부 체크
	bool bPerformNextCombo = true;

	//ExecuteMontageTask 파라미터
	bool bCreateTask = false;

	//Ability 시작 시 캐싱된 Target 정보
	FCurrentTarget CachedTargetInfo;

	//사용되는 태그들
	FGameplayTag EventNotifyRotateToTargetTag;
	FGameplayTag EventNotifyCheckConditionTag;
	FGameplayTag EventNotifyActionRecoveryEndTag;

#pragma endregion

#pragma region "Protected Functions"

	//IHitDetectionUser 인터페이스 구현
	virtual void SetHitDetectionConfig() override;
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	//IMontageAbilityInterface 인터페이스 구현
	UFUNCTION()
	virtual void PlayAction() override;

	UFUNCTION()
	virtual UAnimMontage* SetMontageToPlayTask() override;

	UFUNCTION()
	virtual void ExecuteMontageTask() override;
	
	virtual void BindEventsAndReadyMontageTask() override;

	UFUNCTION()
	virtual void OnTaskMontageCompleted() override;

	UFUNCTION()
	virtual void OnTaskMontageInterrupted() override;

	//노티파이 이벤트를 전부 수신하는 콜백 함수
	UFUNCTION()
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload);

	//RotateToTarget 노티파이 콜백 함수
	UFUNCTION()
	virtual void OnEventRotateToTarget(FGameplayEventData Payload);

	//CheckCondition 노티파이 콜백 함수
	UFUNCTION()
	virtual void OnEventCheckCondition(FGameplayEventData Payload);

	//ActionRecoveryEnd 노티파이 콜백 함수
	UFUNCTION()
	virtual void OnEventActionRecoveryEnd(FGameplayEventData Payload);

	//다음 콤보 실행 함수
	UFUNCTION()
	void PlayNextCombo();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};