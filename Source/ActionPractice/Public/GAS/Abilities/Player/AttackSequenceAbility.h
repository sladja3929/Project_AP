#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "GAS/Abilities/HitDetectionSetter.h"
#include "AttackSequenceAbility.generated.h"

struct FFinalAttackData;
struct FTaggedAttackData;
struct FBlockActionData;
class UWeaponDataAsset;
class UAbilityTask_WaitInputRelease;

//공격 시퀀스 상태
UENUM(BlueprintType)
enum class EAttackSequenceState : uint8
{
	Idle,			//대기 중 (입력 대기)
	WindUp,			//선딜 (차징 이전 구간)
	Charging,		//차징 중
	ChargingMax,	//풀차지 도달
	Attacking,		//공격 실행 중
	Recovery		//후딜 (ActionRecovery 구간)
};

//공격 타입 (어떤 공격이 실행될지)
UENUM(BlueprintType)
enum class EAttackType : uint8
{
	Normal,			//일반 공격
	Charge,			//차지 공격
	Sprint,			//스프린트 공격
	Roll,			//롤 공격
	Jump			//점프 공격
};

/***
 * 모든 공격을 단일 상태머신으로 처리하는 통합 어빌리티.
 * 항상 활성화 상태로 입력/이벤트를 대기하며 상황에 맞는 공격을 실행.
 */
UCLASS()
class ACTIONPRACTICE_API UAttackSequenceAbility : public UActionRecoveryAbility, public IHitDetectionUser
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UAttackSequenceAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//=== 상태 관리 ===
	UPROPERTY()
	EAttackSequenceState CurrentState = EAttackSequenceState::Idle;

	UPROPERTY()
	EAttackType CurrentAttackType = EAttackType::Normal;

	UPROPERTY()
	EAttackType PendingAttackType = EAttackType::Normal;

	//=== 콤보 관리 ===
	UPROPERTY()
	int32 ComboCounter = 0;

	//=== 무기 데이터 ===
	UPROPERTY()
	TObjectPtr<const UWeaponDataAsset> CachedWeaponDataAsset = nullptr;

	//현재 사용 중인 공격 데이터 (WeaponDataAsset 내부 배열 요소 참조)
	const FTaggedAttackData* CurrentAttackData = nullptr;

	//=== 공격 타입별 태그 (WeaponDataAsset에서 검색용) ===
	FGameplayTag AttackTypeNormalTag;
	FGameplayTag AttackTypeChargeTag;
	FGameplayTag AttackTypeSprintTag;
	FGameplayTag AttackTypeRollTag;
	FGameplayTag AttackTypeJumpTag;

	//=== 히트 디텍션 ===
	UPROPERTY()
	FHitDetectionSetter HitDetectionSetter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> DamageInstantEffect;

	//=== 태스크 ===
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask = nullptr;

	//=== 상태 태그 (Local/Auth 분리) ===
	FGameplayTag StateChargingLocalTag;
	FGameplayTag StateChargingAuthTag;
	FGameplayTag StateAttackingLocalTag;
	FGameplayTag StateAttackingAuthTag;

	//=== 이벤트/노티파이 태그 ===
	FGameplayTag EventNotifyResetComboTag;
	FGameplayTag InputAttackTag;
	FGameplayTag InputChargeAttackTag;

	//=== 커브 이름 ===
	static const FName CurveName_ChargeStart;

#pragma endregion

#pragma region "Protected Functions"

	//=== 초기화 ===
	virtual void ActivateInitSettings() override;
	void CacheWeaponData();
	void CacheGameplayTags();
	void BindHitDetectionSetter();
	//=== 상태 태그 관리 ===
	void AddStateTag(const FGameplayTag& LocalTag, const FGameplayTag& AuthTag);
	void RemoveStateTag(const FGameplayTag& LocalTag, const FGameplayTag& AuthTag);
	void ClearAllStateTags();


	virtual bool ConsumeStamina() override;
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void ExecuteMontageTask() override;
	virtual void BindEventsAndReadyMontageTask() override;

	//=== 히트 디텍션 (IHitDetectionUser) ===
	virtual void SetHitDetectionConfig() override;
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	//=== 이벤트 핸들러 ===
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskNotifyEventsReceived(FGameplayEventData Payload) override;
	virtual void OnEventInputByBuffer(FGameplayEventData Payload) override;
	virtual void OnActionRecoveryEnd() override;

	//=== 커브 에지 핸들러 ===
	virtual void OnCurveRisingEdgeReceived(FName CurveName) override;
	virtual void OnCurveFallingEdgeReceived(FName CurveName) override;

	//=== 차지 공격 전용 ===

	UFUNCTION()
	void HandleWaitInputReleased(float TimeHeld);

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	//스태미나 비용 계산 (차지 보너스 등 적용)
	float CalculateStaminaCost() const;


#pragma endregion
};