#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAbility.h"
#include "GAS/Abilities/HitDetectionSetter.h"
#include "GAS/Abilities/MontageAbilityInterface.h"
#include "EnemyAttackAbility.generated.h"

class UAbilityTask_PlayMontageWithEvents;
struct FFinalAttackData;

/***
 * EnemyAttack은 플레이어와 달리 공격 몽타주는 하나
 * 단, 하나의 몽타주에서 현재 콤보 정보를 알 수 있도록 노티파이 부착
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotation")
	float RotateTime = 0.1f;

	//콤보 카운터
	int32 ComboCounter = -1;

	//사용되는 태그들
	FGameplayTag EventNotifyRotateToTargetTag;
	FGameplayTag EventNotifyAddComboTag;

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

	//AddCombo 노티파이 콜백 함수
	UFUNCTION()
	virtual void OnEventAddCombo(FGameplayEventData Payload);

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};