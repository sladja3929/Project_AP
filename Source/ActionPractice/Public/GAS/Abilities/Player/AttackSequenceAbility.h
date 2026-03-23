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
	None,
	Idle,			//대기 중 (입력 대기)
	Prepare,		//공격 준비
	Attacking,		//공격 실행 중
	AfterRecovery	//후딜 (ActionRecoveryEnd 이후)
};

//공격 타입
UENUM(BlueprintType)
enum class EAttackType : uint8
{
	None,
	Normal,			//일반 공격
	Charge,			//차지 공격
	Sprint,			//스프린트 공격
	ChargeSprint,
	Roll			//롤 공격
};

//차지 상태
UENUM(BlueprintType)
enum class EChargeProgress : uint8
{
	WindUp, 	//차지 이전
	NoCharge,	//차지 안함
	Charging,	//차지 중
	MaxCharged	//최대 차지
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
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	// ===== 상태 관리 =====
	UPROPERTY()
	EAttackSequenceState CurrentState = EAttackSequenceState::Idle;

	UPROPERTY()
	EAttackSequenceState PreviousState = EAttackSequenceState::None;
	
	UPROPERTY()
	EAttackType CurrentAttackType = EAttackType::Normal;

	UPROPERTY()
	EAttackType PreviousAttackType = EAttackType::Normal;

	UPROPERTY()
	FGameplayTagContainer CurrentAttackTags;
	
	UPROPERTY()
	EChargeProgress CurrentChargeProgress = EChargeProgress::NoCharge;
	
	UPROPERTY()
	int32 ComboCounter = 0;

	UPROPERTY()
	int32 MaxComboCount = 0;

	// ===== 버퍼 입력 큐 (서버용) =====
	//서버에서 Attacking 상태일 때 도착한 InputByBuffer를 예약
	UPROPERTY()
	bool bHasPendingBufferInput = false;

	UPROPERTY()
	FGameplayEventData PendingBufferPayload;

	//스태미나 부족으로 Idle 복귀 시 몽타주 유지 플래그
	bool bPreserveMontage = false;

	// ===== 무기 데이터 =====
	UPROPERTY()
	TObjectPtr<const UWeaponDataAsset> CachedWeaponDataAsset = nullptr;
	
	//현재 사용 중인 공격 데이터
	const FTaggedAttackData* CurrentAttackData = nullptr;

	// ===== 히트 디텍션 =====
	UPROPERTY()
	FHitDetectionSetter HitDetectionSetter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<UGameplayEffect> DamageInstantEffect;

	// 공격 시작 시 취소할 어빌리티 태그 (AbilityTagsToCancel 대체 - 이미 활성화된 어빌리티라 직접 처리)
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	FGameplayTagContainer AbilityTagsToCancelOnAttack;

	// ===== 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitAttackInputEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitResetComboEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitCancelAttackEventTask = nullptr;
	
	// ===== 게임플레이 태그 =====

	//입력 태그
	FGameplayTag InputAttackTag;
	FGameplayTag InputChargeAttackTag;

	//어빌리티 태그
	FGameplayTag AbilityAttackNormalTag;
	FGameplayTag AbilityAttackChargeTag;
	FGameplayTag AbilityAttackSprintTag;
	FGameplayTag AbilityAttackRollTag;
	
	//상태 태그
	FGameplayTag StateRollingTag;
	FGameplayTag StateJustRolledTag;
	FGameplayTag StateSprintingTag;
	FGameplayTag StateAbilityAttackingAuthTag;
	FGameplayTag StateAbilityAttackingLocalTag;

	//이벤트 태그
	FGameplayTag EventActionAttackInputTag;
	FGameplayTag EventActionCancelAttackTag;
	FGameplayTag EventNotifyResetComboTag;
	

	// ===== 커브 이름 =====
	static const FName CurveName_ChargeStart;

#pragma endregion

#pragma region "Protected Functions"

	// ===== 설정 / 초기화 =====
	virtual void ActivateInitSettings() override;
	void CacheWeaponData();
	void CancelAbilitiesOnAttack();
	void CacheGameplayTags();
	void BindHitDetectionSetter();
	void StopMontageAndEndTask();
	void AddOrRemoveGameplayTag(const FGameplayTag Auth, const FGameplayTag Local, bool bAdd);
	
	// ===== 상태 관리 =====
	void ChangeAttackType(const EAttackType NewType);
	void ChangeState(const EAttackSequenceState NewState);
	virtual bool ConsumeStamina() override;
	
	// ===== 몽타주 태스크 =====
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;

	// ===== 히트 디텍션 (IHitDetectionUser) ======
	virtual void SetHitDetectionConfig() override;
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	// ===== 핸들러 함수 =====
	//이벤트 핸들러
	UFUNCTION()
	void OnEventAttackInput(FGameplayEventData Payload); //InputPressed 대체 이벤트, IA_Attack과 IA_ChargeAttack 모두 수신

	void ProcessNormalAttackInput(const FGameplayEventData& Payload);
	void ProcessChargeAttackInput(const FGameplayEventData& Payload);

	virtual void OnWaitInputRelease(float TimeHeld) override; 
	virtual void OnEventInputByBuffer(FGameplayEventData Payload) override;

	//예약된 버퍼 입력 소비 (AfterRecovery 진입 시 호출)
	void ConsumePendingBufferInput();

	UFUNCTION()
	void OnEventCancelAttack(FGameplayEventData Payload);
	
	UFUNCTION()
	void OnEventResetCombo(FGameplayEventData Payload);
	
	//몽타주 핸들러
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskMontageInterrupted() override;
	
	//커브 에지 핸들러
	virtual void OnCurveRisingEdgeReceived(FName CurveName) override;
	virtual void OnCurveFallingEdgeReceived(FName CurveName) override;
	virtual void ProcessActionRecoveryEnd() override;


#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	//스태미나 비용 계산 (차지 보너스 등 적용)
	//float CalculateStaminaCost() const;


#pragma endregion
};