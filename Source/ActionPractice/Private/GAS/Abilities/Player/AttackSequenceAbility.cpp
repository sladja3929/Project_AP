#include "GAS/Abilities/Player/AttackSequenceAbility.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GAS/Abilities/HitDetectionSetter.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAttackSequenceAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAttackSequenceAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

//커브 이름 상수 정의
const FName UAttackSequenceAbility::CurveName_ChargeStart = TEXT("ChargeStart");

UAttackSequenceAbility::UAttackSequenceAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bRetriggerInstancedAbility = false;

	StaminaCost = 15.0f;
}

void UAttackSequenceAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	CacheGameplayTags();
}

void UAttackSequenceAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	DEBUG_LOG(TEXT("AttackSequenceAbility Activated"));
}

#pragma region "Ability Activate Initialization Functions"

void UAttackSequenceAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	//무기 데이터 캐싱
	CacheWeaponData();

	//HitDetectionSetter 바인딩
	BindHitDetectionSetter();
	
	//InputByBuffer 태스크 준비
	ReadyInputByBufferTask();

	//상태 초기화
	CurrentState = EAttackSequenceState::Idle;
}

void UAttackSequenceAbility::CacheGameplayTags()
{
	//상태 태그
	/*StateChargingLocalTag = UGameplayTagsSubsystem::GetStateChargingLocalTag();
	StateChargingAuthTag = UGameplayTagsSubsystem::GetStateChargingAuthTag();
	StateAttackingLocalTag = UGameplayTagsSubsystem::GetStateAttackingLocalTag();
	StateAttackingAuthTag = UGameplayTagsSubsystem::GetStateAttackingAuthTag();*/

	//이벤트/노티파이 태그
	EventNotifyResetComboTag = UGameplayTagsSubsystem::GetEventNotifyResetComboTag();
	InputAttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Attack")));
	InputChargeAttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.ChargeAttack")));

	//공격 타입 태그 (WeaponDataAsset 검색용)
	AttackTypeNormalTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Attack.Normal")));
	AttackTypeChargeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Attack.Charge")));
	AttackTypeSprintTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Attack.Sprint")));
	AttackTypeRollTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Attack.Roll")));
	AttackTypeJumpTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Attack.Jump")));

	//태그 유효성 검사
	if (!EventNotifyResetComboTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyResetComboTag is not valid"));
	}
}

void UAttackSequenceAbility::CacheWeaponData()
{
	//WeaponDataAsset 전체 캐싱
	CachedWeaponDataAsset = FWeaponAbilityStatics::GetWeaponDataAssetFromAbility(this, false);

	if (!CachedWeaponDataAsset)
	{
		DEBUG_LOG(TEXT("Failed to cache WeaponDataAsset"));
		return;
	}

	DEBUG_LOG(TEXT("WeaponDataAsset cached - AttackData count: %d"), CachedWeaponDataAsset->TaggedAttackData.Num());
}

void UAttackSequenceAbility::BindHitDetectionSetter()
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character)
	{
		DEBUG_LOG(TEXT("No Character"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
    
	//HitDetectionSetter 초기화
	if (!HitDetectionSetter.Init(Character->GetHitDetectionInterface()))
	{
		DEBUG_LOG(TEXT("Failed to init HitDetectionSetter"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//HitDetectionSetter 바인딩
	if (!HitDetectionSetter.Bind(this))
	{
		DEBUG_LOG(TEXT("Failed to bind HitDetectionSetter"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
}

#pragma endregion

void UAttackSequenceAbility::AddStateTag(const FGameplayTag& LocalTag, const FGameplayTag& AuthTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	ACharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	if (Character->IsLocallyControlled() && LocalTag.IsValid())
	{
		ASC->AddLooseGameplayTag(LocalTag);
	}

	if (Character->HasAuthority() && AuthTag.IsValid())
	{
		ASC->AddLooseGameplayTag(AuthTag);
	}
}

void UAttackSequenceAbility::RemoveStateTag(const FGameplayTag& LocalTag, const FGameplayTag& AuthTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	ACharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	if (Character->IsLocallyControlled() && LocalTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(LocalTag);
	}

	if (Character->HasAuthority() && AuthTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(AuthTag);
	}
}

void UAttackSequenceAbility::ClearAllStateTags()
{
	RemoveStateTag(StateChargingLocalTag, StateChargingAuthTag);
	RemoveStateTag(StateAttackingLocalTag, StateAttackingAuthTag);
}


void UAttackSequenceAbility::ExecuteAttack(EAttackType AttackType)
{
	
}

void UAttackSequenceAbility::PlayNextCombo()
{
	
}

bool UAttackSequenceAbility::ConsumeStamina()
{
	
}

float UAttackSequenceAbility::CalculateStaminaCost() const
{
	
}

UAnimMontage* UAttackSequenceAbility::SetMontageToPlayTask()
{
	
}

void UAttackSequenceAbility::ExecuteMontageTask()
{
	
}

void UAttackSequenceAbility::BindEventsAndReadyMontageTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("BindEventsAndReadyMontageTask: No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//부모 클래스 바인딩 (ActionRecovery, EnableBufferInput 커브 등)
	Super::BindEventsAndReadyMontageTask();

	//ChargeStart 커브 추가 (차지 공격용)
	if (CurrentAttackType == EAttackType::Charge)
	{
		PlayMontageWithEventsTask->AddCurveToPolling(CurveName_ChargeStart);
	}
}

#pragma endregion

#pragma region "Hit Detection"

void UAttackSequenceAbility::SetHitDetectionConfig()
{
	
}

void UAttackSequenceAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
	
}

#pragma endregion

void UAttackSequenceAbility::OnTaskMontageCompleted()
{
	
}

void UAttackSequenceAbility::OnCurveRisingEdgeReceived(FName CurveName)
{
	Super::OnCurveRisingEdgeReceived(CurveName);

	if (CurveName == CurveName_ChargeStart)
	{
		
	}
}

void UAttackSequenceAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	DEBUG_LOG(TEXT("AttackSequenceAbility Cancelled"));
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UAttackSequenceAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//상태 태그 정리
	ClearAllStateTags();

	//태스크 정리
	StopWaitInputReleaseTask();

	//상태 초기화
	CurrentState = EAttackSequenceState::Idle;
	ComboCounter = 0;
	bMaxCharged = false;
	bReleasedBeforeChargeStart = false;
	CurrentAttackData = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	DEBUG_LOG(TEXT("AttackSequenceAbility Ended - Cancelled: %s"), bWasCancelled ? TEXT("true") : TEXT("false"));
}