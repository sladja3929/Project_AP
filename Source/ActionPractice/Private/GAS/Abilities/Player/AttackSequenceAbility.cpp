#include "GAS/Abilities/Player/AttackSequenceAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
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

	//항상 대기: 입력 이벤트, 버퍼 이벤트
	START_WAIT_EVENT_TASK(WaitAttackInputEventTask, EventActionAttackInputTag, OnEventAttackInput, nullptr, false, true);
	START_WAIT_EVENT_TASK(WaitInputByBufferEventTask, EventInputByBufferTag, OnEventInputByBuffer, nullptr, false, true);
}

#pragma region "Activate Initialization Functions"

void UAttackSequenceAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	//무기 데이터 캐싱
	CacheWeaponData();

	//HitDetectionSetter 바인딩
	BindHitDetectionSetter();

	if (!CachedWeaponDataAsset || !HitDetectionSetter.IsValid())
	{
		DEBUG_LOG(TEXT("AttackSequence prerequisites not ready. EndAbility. WeaponData=%s HitDetectionValid=%s"),
			CachedWeaponDataAsset ? TEXT("true") : TEXT("false"),
			HitDetectionSetter.IsValid() ? TEXT("true") : TEXT("false"));

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//타입 초기화
	CurrentAttackType = EAttackType::None;
	PreviousAttackType = EAttackType::None;
	CurrentAttackTags.Reset();
	
	//상태 초기화
	CurrentState = EAttackSequenceState::Idle;
	PreviousState = EAttackSequenceState::None;
	CurrentChargeProgress = EChargeProgress::NoCharge;
}

void UAttackSequenceAbility::CacheGameplayTags()
{
	//입력 태그
	InputAttackTag = UGameplayTagsSubsystem::GetInputAttackTag();
	InputChargeAttackTag = UGameplayTagsSubsystem::GetInputChargeAttackTag();

	//어빌리티 태그
	AbilityAttackNormalTag = UGameplayTagsSubsystem::GetAbilityAttackNormalTag();
	AbilityAttackChargeTag = UGameplayTagsSubsystem::GetAbilityAttackChargeTag();
	AbilityAttackSprintTag = UGameplayTagsSubsystem::GetAbilityAttackSprintTag();
	AbilityAttackRollTag = UGameplayTagsSubsystem::GetAbilityAttackRollTag();

	//상태 태그
	StateRollingTag = UGameplayTagsSubsystem::GetStateAbilityRollingTag();
	StateJustRolledTag = UGameplayTagsSubsystem::GetStateAbilityJustRolledTag();
	StateSprintingTag = UGameplayTagsSubsystem::GetStateAbilitySprintingTag();
	StateAbilityAttackingAuthTag = UGameplayTagsSubsystem::GetStateAbilityAttackingAuthTag();
	StateAbilityAttackingLocalTag = UGameplayTagsSubsystem::GetStateAbilityAttackingLocalTag();

	//이벤트 태그
	EventActionAttackInputTag = UGameplayTagsSubsystem::GetEventActionAttackInputTag();
	EventActionCancelAttackTag = UGameplayTagsSubsystem::GetEventActionCancelAttackTag();
	EventNotifyResetComboTag = UGameplayTagsSubsystem::GetEventNotifyResetComboTag();

	//태그 유효성 검사 - 입력 태그
	if (!InputAttackTag.IsValid())
	{
		DEBUG_LOG(TEXT("InputAttackTag is not valid"));
	}
	if (!InputChargeAttackTag.IsValid())
	{
		DEBUG_LOG(TEXT("InputChargeAttackTag is not valid"));
	}

	//태그 유효성 검사 - 어빌리티 태그
	if (!AbilityAttackNormalTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityAttackNormalTag is not valid"));
	}
	if (!AbilityAttackChargeTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityAttackChargeTag is not valid"));
	}
	if (!AbilityAttackSprintTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityAttackSprintTag is not valid"));
	}
	if (!AbilityAttackRollTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityAttackRollTag is not valid"));
	}

	//태그 유효성 검사 - 상태 태그
	if (!StateRollingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateRollingTag is not valid"));
	}
	if (!StateJustRolledTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateJustRolledTag is not valid"));
	}
	if (!StateSprintingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateSprintingTag is not valid"));
	}
	if (!StateAbilityAttackingAuthTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityAttackingAuthTag is not valid"));
	}
	if (!StateAbilityAttackingLocalTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityAttackingLocalTag is not valid"));
	}

	//태그 유효성 검사 - 이벤트 태그
	if (!EventActionAttackInputTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventActionAttackInputTag is not valid"));
	}
	if (!EventActionCancelAttackTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventActionCancelAttackTag is not valid"));
	}
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

	//패키지 빌드에서 SoftPtr 미로드 방지 — DA 캐싱 시점에 몽타주 프리로드
	const_cast<UWeaponDataAsset*>(CachedWeaponDataAsset.Get())->PreloadAllMontages();

	DEBUG_LOG(TEXT("WeaponDataAsset cached - AttackData count: %d"), CachedWeaponDataAsset->TaggedAttackData.Num());
}

void UAttackSequenceAbility::BindHitDetectionSetter()
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character)
	{
		DEBUG_LOG(TEXT("No Character"));
		return;
	}
    
	//HitDetectionSetter 초기화
	if (!HitDetectionSetter.Init(Character->GetHitDetectionInterface()))
	{
		DEBUG_LOG(TEXT("Failed to init HitDetectionSetter"));
		return;
	}

	//HitDetectionSetter 바인딩
	if (!HitDetectionSetter.Bind(this))
	{
		DEBUG_LOG(TEXT("Failed to bind HitDetectionSetter"));
		return;
	}
}

void UAttackSequenceAbility::StopMontageAndEndTask()
{
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}
}

void UAttackSequenceAbility::CancelAbilitiesOnAttack()
{
	if (AbilityTagsToCancelOnAttack.IsEmpty()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	ASC->CancelAbilities(&AbilityTagsToCancelOnAttack);
}

void UAttackSequenceAbility::AddOrRemoveGameplayTag(const FGameplayTag Auth, const FGameplayTag Local, bool bAdd)
{
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		return;
	}
	if (bAdd) APASC->AddTag_NetPredicted(Auth, Local);
	else APASC->RemoveTags_NetPredicted(Auth, Local);
}

#pragma endregion

#pragma region "Hit Detection"

void UAttackSequenceAbility::SetHitDetectionConfig()
{
	//HitDetectionSetter가 바인딩되지 않았으면 재시도
	if (!HitDetectionSetter.IsValid())
	{
		DEBUG_LOG(TEXT("HitDetectionSetter not bound, retrying..."));
		BindHitDetectionSetter();
	}

	//PrepareHitDetection 호출
	if (!HitDetectionSetter.PrepareHitDetection(CurrentAttackTags, ComboCounter))
	{
		DEBUG_LOG(TEXT("Failed to prepare HitDetection"));
		StopMontageAndEndTask();
		return;
	}
}

void UAttackSequenceAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
	//Source ASC (공격자, AttackAbility 소유자)
	UActionPracticeAbilitySystemComponent* SourceASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!HitActor || !SourceASC) return;

	//Target ASC (피격자)
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC) return;

	//Source ASC에서 GE Spec 생성 (HitResult 포함 — Cue에 피격 위치/방향 전달)
	FGameplayEffectSpecHandle SpecHandle = SourceASC->CreateAttackGameplayEffectSpec(DamageInstantEffect, GetAbilityLevel(), this, AttackData, &HitResult);
    
	if (SpecHandle.IsValid())
	{        
		//Target에게 적용
		FActiveGameplayEffectHandle ActiveGEHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        
		//적용에 성공했으면
		if (ActiveGEHandle.WasSuccessfullyApplied())
		{
			//추후 필요시 구현
		}
	}
}

#pragma endregion

#pragma region "State Management Functions"
void UAttackSequenceAbility::ChangeAttackType(const EAttackType NewType)
{
	PreviousAttackType = CurrentAttackType;
	CurrentAttackType = NewType;
	DEBUG_LOG(TEXT("AttackSequenceAbility - Attack Type Changed: %s -> %s"),
		*UEnum::GetValueAsName(PreviousAttackType).ToString(),
		*UEnum::GetValueAsName(CurrentAttackType).ToString());
	
	if (PreviousAttackType == CurrentAttackType) return;

	DEBUG_LOG(TEXT("New Type Diff - MaxComboCount: %d, ComboCounter Reset"), MaxComboCount);

	CurrentAttackTags.Reset();

	switch (CurrentAttackType)
	{
	case EAttackType::Normal:
		CurrentAttackTags.AddTag(AbilityAttackNormalTag);
		break;
		
	case EAttackType::Charge:
		CurrentAttackTags.AddTag(AbilityAttackChargeTag);
		break;
		
	case EAttackType::Sprint:
		CurrentAttackTags.AddTag(AbilityAttackNormalTag);
		CurrentAttackTags.AddTag(AbilityAttackSprintTag);
		break;

	case EAttackType::ChargeSprint:
		CurrentAttackTags.AddTag(AbilityAttackChargeTag);
		CurrentAttackTags.AddTag(AbilityAttackSprintTag);
		break;
		
	case EAttackType::Roll:
		CurrentAttackTags.AddTag(AbilityAttackNormalTag);
		CurrentAttackTags.AddTag(AbilityAttackRollTag);
		break;
		
	default:
		break;	
	}
	
	CurrentAttackData = FWeaponAbilityStatics::GetAttackDataByTags(CachedWeaponDataAsset, CurrentAttackTags);
	MaxComboCount = CurrentAttackData ? CurrentAttackData->ComboSequence.Num() : 0;
	ComboCounter = 0;
}

void UAttackSequenceAbility::ChangeState(const EAttackSequenceState NewState)
{
	PreviousState = CurrentState;
	CurrentState = NewState;
	DEBUG_LOG(TEXT("AttackSequenceAbility - State Changed: %s -> %s"),
		*UEnum::GetValueAsName(PreviousState).ToString(),
		*UEnum::GetValueAsName(CurrentState).ToString());

	if (PreviousState == CurrentState) return;

	// PreviousState 퇴장 로직
	switch (PreviousState)
	{
	case EAttackSequenceState::Idle:
		break;
		
	case EAttackSequenceState::Prepare:
		break;
		
	case EAttackSequenceState::Attacking:
		
		break;
		
	case EAttackSequenceState::AfterRecovery:
		END_ABILITY_TASK(WaitResetComboEventTask);
		END_ABILITY_TASK(WaitCancelAttackEventTask);
		break;
		
	default:
		break;
	}

	// CurrentState 진입 로직
	switch (CurrentState)
	{
	case EAttackSequenceState::Idle:
		//None: ComboCounter, MaxComboCount 0
		ChangeAttackType(EAttackType::None);
		CurrentChargeProgress = EChargeProgress::NoCharge;

		//스태미나 부족 캔슬일 경우
		if (bPreserveMontage)
		{
			//스태미나 부족으로 복귀: 몽타주와 Attacking 태그 유지 (이동 캔슬 유지)
			bPreserveMontage = false;

			//이동으로 인한 후딜 취소 이벤트 리스닝
			START_WAIT_EVENT_TASK(WaitCancelAttackEventTask, EventActionCancelAttackTag, OnEventCancelAttack, nullptr, true, true);
		}
		
		else
		{
			AddOrRemoveGameplayTag(StateAbilityAttackingAuthTag, StateAbilityAttackingLocalTag, false);
			StopMontageAndEndTask();
		}
		
		break;

	case EAttackSequenceState::Prepare:
		//스테미나 부족시 Idle 복귀 (몽타주 유지)
		if (!ConsumeStamina())
		{
			bPreserveMontage = true;
			ChangeState(EAttackSequenceState::Idle);
			break;
		}
		
		CancelAbilitiesOnAttack();
		StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();
		if (CurrentChargeProgress != EChargeProgress::NoCharge) StartWaitInputReleaseTask(true);
		break;

	case EAttackSequenceState::Attacking:
		//스테미나 부족시 Idle 복귀 (몽타주 유지)
		if (!ConsumeStamina())
		{
			bPreserveMontage = true;
			ChangeState(EAttackSequenceState::Idle);
			break;
		}
		
		CancelAbilitiesOnAttack();
		SetHitDetectionConfig();
		AddOrRemoveGameplayTag(StateAbilityAttackingAuthTag, StateAbilityAttackingLocalTag, true);
		StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();
		break;

	case EAttackSequenceState::AfterRecovery:
		//리커버리 직후 콤보 카운터 증가
		//리커버리 종료 -> 콤보 카운터 중가 -> InputByBuffer 또는 AttackInput-> 다른 공격이면 콤보 초기화 / 같은 공격이면 다음 콤보 실행
		ComboCounter++;
		if (ComboCounter >= MaxComboCount) ComboCounter = 0;

		//콤보 리셋 이벤트
		START_WAIT_EVENT_TASK(WaitResetComboEventTask, EventNotifyResetComboTag, OnEventResetCombo, nullptr, true, true);

		//이동으로 인한 후딜 취소 이벤트
		START_WAIT_EVENT_TASK(WaitCancelAttackEventTask, EventActionCancelAttackTag, OnEventCancelAttack, nullptr, true, true);

		//서버: 예약된 버퍼 입력이 있으면 즉시 소비
		if (GetAvatarActorFromActorInfo()->HasAuthority())
		{
			ConsumePendingBufferInput();
		}
		break;

	default:
		break;
	}
}

bool UAttackSequenceAbility::ConsumeStamina()
{
	if (CurrentState == EAttackSequenceState::Attacking)
	{
		SetStaminaCost(CurrentAttackData->ComboSequence[ComboCounter].AttackData.StaminaCost);
	}

	else SetStaminaCost(0);
	
	if (!ApplyStaminaCost())
	{
		DEBUG_LOG(TEXT("No Stamina"));
		return false;
	}

	return true;
}
#pragma endregion

#pragma region "Task Functions"
UAnimMontage* UAttackSequenceAbility::SetMontageToPlayTask()
{	
	if (CurrentAttackData && CurrentAttackData->ComboSequence.IsValidIndex(ComboCounter))
	{
		// 소프트 레퍼런스를 실제 오브젝트로 로드
		const auto& ComboData = CurrentAttackData->ComboSequence[ComboCounter];
		UAnimMontage* Montage = nullptr;

		//공격 몽타주 로드
		if (CurrentState == EAttackSequenceState::Attacking)
		{
			Montage = ComboData.AttackMontage.LoadSynchronous();
		}

		//차지 몽타주 로드
		else if (CurrentState == EAttackSequenceState::Prepare)
		{
			Montage = ComboData.SubAttackMontage.LoadSynchronous();
		}
		
		if (!Montage)
		{
			DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load montage. ComboIndex=%d"), ComboCounter);
			return nullptr;
		}

		return Montage;
	}

	return nullptr;
}

void UAttackSequenceAbility::SetUpPlayMontageWithEventsTask()
{
	//부모 클래스 바인딩 (ActionRecovery, EnableBufferInput 커브 등)
	Super::SetUpPlayMontageWithEventsTask();

	//ChargeStart 커브 추가 (차지 공격용)
	if (CurrentAttackType == EAttackType::Charge)
	{
		PlayMontageWithEventsTask->EnableCurvePolling(CurveName_ChargeStart);
	}
}
#pragma endregion

#pragma region "Hander Functions"
void UAttackSequenceAbility::OnEventAttackInput(FGameplayEventData Payload)
{
	DEBUG_LOG(TEXT("AttackSequenceAbility - AttackInput Received"));

	//Normal Attack 입력
	if (Payload.InstigatorTags.HasTag(InputAttackTag))
	{
		ProcessNormalAttackInput(Payload);
	}

	//Charge Attack 입력
	else if (Payload.InstigatorTags.HasTag(InputChargeAttackTag))
	{
		CurrentChargeProgress = EChargeProgress::WindUp;
		ProcessChargeAttackInput(Payload);
	}
}

void UAttackSequenceAbility::ProcessNormalAttackInput(const FGameplayEventData& Payload)
{
	//Payload의 태그로 공격 타입 판정 (버퍼 저장 시점 기준)
	if (Payload.InstigatorTags.HasTag(StateJustRolledTag) || Payload.InstigatorTags.HasTag(StateRollingTag))
	{
		ChangeAttackType(EAttackType::Roll);
	}
	
	else if (Payload.InstigatorTags.HasTag(StateSprintingTag))
	{
		ChangeAttackType(EAttackType::Sprint);
	}
	
	else
	{
		ChangeAttackType(EAttackType::Normal);
	}

	ChangeState(EAttackSequenceState::Attacking);
}

void UAttackSequenceAbility::ProcessChargeAttackInput(const FGameplayEventData& Payload)
{
	//Payload의 태그로 공격 타입 판정 (버퍼 저장 시점 기준)
	if (Payload.InstigatorTags.HasTag(StateJustRolledTag) || Payload.InstigatorTags.HasTag(StateRollingTag))
	{
		ChangeAttackType(EAttackType::Roll);
		ChangeState(EAttackSequenceState::Attacking);
	}
	
	else if (Payload.InstigatorTags.HasTag(StateSprintingTag))
	{
		ChangeAttackType(EAttackType::ChargeSprint);
		ChangeState(EAttackSequenceState::Attacking);
	}
	
	else
	{
		ChangeAttackType(EAttackType::Charge);
		ChangeState(EAttackSequenceState::Prepare);
	}	
}

void UAttackSequenceAbility::OnWaitInputRelease(float TimeHeld)
{
	if (CurrentState != EAttackSequenceState::Prepare) return;

	//차지 상태 이전 입력 해제: 차지 트리거시 즉시 공격
	if (CurrentChargeProgress == EChargeProgress::WindUp)
	{
		CurrentChargeProgress = EChargeProgress::NoCharge;
	}

	//차지 중 입력 해제: 즉시 차지 공격 실행
	else if (CurrentChargeProgress == EChargeProgress::Charging)
	{
		CurrentChargeProgress = EChargeProgress::NoCharge;
		ChangeState(EAttackSequenceState::Attacking);
	}
}

void UAttackSequenceAbility::OnEventInputByBuffer(FGameplayEventData Payload)
{
	DEBUG_LOG(TEXT("InputByBuffer Received - CurrentState: %s"),
		*UEnum::GetValueAsName(CurrentState).ToString());

	//서버에서 Attacking 상태일 때 → 예약만 하고 return
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor && AvatarActor->HasAuthority())
	{
		if (CurrentState == EAttackSequenceState::Attacking)
		{
			DEBUG_LOG(TEXT("Server - Buffering InputByBuffer (State: Attacking)"));
			bHasPendingBufferInput = true;
			PendingBufferPayload = Payload;
			return;
		}
	}

	//기존 로직 (클라이언트 예측 또는 서버 AfterRecovery 상태)
	if (Payload.InstigatorTags.HasTag(InputAttackTag))
	{
		ProcessNormalAttackInput(Payload);
	}
	else if (Payload.InstigatorTags.HasTag(InputChargeAttackTag))
	{
		//EventMagnitude로 단발인지 홀드인지 판별
		if (Payload.EventMagnitude != 0.0f)
		{
			DEBUG_LOG(TEXT("Input By Buffer - No Charge true"));
			CurrentChargeProgress = EChargeProgress::NoCharge;
		}
		else
		{
			CurrentChargeProgress = EChargeProgress::WindUp;
		}

		ProcessChargeAttackInput(Payload);
	}
}

void UAttackSequenceAbility::ConsumePendingBufferInput()
{
	if (!bHasPendingBufferInput)
	{
		return;
	}

	DEBUG_LOG(TEXT("ConsumePendingBufferInput - Processing Buffered Input"));

	bHasPendingBufferInput = false;

	//예약된 Payload로 기존 로직 실행
	const FGameplayEventData& Payload = PendingBufferPayload;

	if (Payload.InstigatorTags.HasTag(InputAttackTag))
	{
		ProcessNormalAttackInput(Payload);
	}
	
	else if (Payload.InstigatorTags.HasTag(InputChargeAttackTag))
	{
		//EventMagnitude로 단발인지 홀드인지 판별
		if (Payload.EventMagnitude != 0.0f)
		{
			CurrentChargeProgress = EChargeProgress::NoCharge;
		}
		
		else
		{
			CurrentChargeProgress = EChargeProgress::WindUp;
		}

		ProcessChargeAttackInput(Payload);
	}

	//Payload 초기화
	PendingBufferPayload = FGameplayEventData();
}

void UAttackSequenceAbility::OnEventCancelAttack(FGameplayEventData Payload)
{
	DEBUG_LOG(TEXT("AttackSequenceAbility - CancelAttack Notify Received"));

	//이미 Idle 상태(스태미나 부족 복귀)일 때는 ChangeState 가드에 걸리므로 직접 정리
	if (CurrentState == EAttackSequenceState::Idle)
	{
		AddOrRemoveGameplayTag(StateAbilityAttackingAuthTag, StateAbilityAttackingLocalTag, false);
		StopMontageAndEndTask();
		return;
	}

	ChangeState(EAttackSequenceState::Idle);
}

void UAttackSequenceAbility::OnEventResetCombo(FGameplayEventData Payload)
{
	DEBUG_LOG(TEXT("AttackSequenceAbility - ResetCombo Notify Received"));
	ComboCounter = 0;
}

void UAttackSequenceAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("AttackSequenceAbility - Task Montage Completed"));
	//차지 몽타주가 Completed
	if (CurrentState == EAttackSequenceState::Prepare && CurrentChargeProgress == EChargeProgress::Charging)
	{
		DEBUG_LOG(TEXT("AttackSequenceAbility - Charge Montage Completed, Max Charged"));
		CurrentChargeProgress = EChargeProgress::MaxCharged;
		ChangeState(EAttackSequenceState::Attacking);
	}

	//공격 몽타주가 Completed
	else if (CurrentState == EAttackSequenceState::Attacking)
	{
		DEBUG_LOG(TEXT("AttackSequenceAbility - Attack Montage Completed, Idle"));
		ChangeState(EAttackSequenceState::Idle);
	}
}

void UAttackSequenceAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("AttackSequenceAbility - Task Montage Interrupted"));

	//피격 인터럽트: 버퍼 입력 초기화
	bHasPendingBufferInput = false;
	PendingBufferPayload = FGameplayEventData();

	//차지 진행 초기화
	CurrentChargeProgress = EChargeProgress::NoCharge;

	//Idle로 강제 복귀 (ChangeAttackType(None), 태그 제거, StopMontageAndEndTask 일괄 처리)
	ChangeState(EAttackSequenceState::Idle);
}

void UAttackSequenceAbility::OnCurveRisingEdgeReceived(FName CurveName)
{
	Super::OnCurveRisingEdgeReceived(CurveName);

	if (CurveName == CurveName_ChargeStart)
	{
		if (CurrentState != EAttackSequenceState::Prepare) return;
		
		if (CurrentChargeProgress == EChargeProgress::WindUp)
		{
			DEBUG_LOG(TEXT("AttackSequenceAbility - Charge Start (Curve Rising Edge)"));
			CurrentChargeProgress = EChargeProgress::Charging;
		}

		else if (CurrentChargeProgress == EChargeProgress::NoCharge)
		{
			DEBUG_LOG(TEXT("AttackSequenceAbility - No Charge"));
			ChangeState(EAttackSequenceState::Attacking);
		}
	}
}

void UAttackSequenceAbility::OnCurveFallingEdgeReceived(FName CurveName)
{
	Super::OnCurveFallingEdgeReceived(CurveName);
}

void UAttackSequenceAbility::ProcessActionRecoveryEnd()
{
	DEBUG_LOG(TEXT("AttackSequenceAbility - Action Recovery Ended, ComboCounter: %d"), ComboCounter);
	ChangeState(EAttackSequenceState::AfterRecovery);
	
	Super::ProcessActionRecoveryEnd();
}
#pragma endregion

void UAttackSequenceAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	DEBUG_LOG(TEXT("AttackSequenceAbility Cancelled"));
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UAttackSequenceAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//디버그: EndAbility 호출 추적
	DEBUG_LOG(TEXT("AttackSequenceAbility::EndAbility called - bWasCancelled: %s, bReplicateEndAbility: %s, ActivationInfo.ActivationMode: %d, PredictionKey: %s"),
		bWasCancelled ? TEXT("true") : TEXT("false"),
		bReplicateEndAbility ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ActivationInfo.ActivationMode),
		*ActivationInfo.GetActivationPredictionKey().ToString());

	//태스크 초기화
	StopMontageAndEndTask();
	END_ABILITY_TASK(WaitAttackInputEventTask);
	END_ABILITY_TASK(WaitInputByBufferEventTask);
	END_ABILITY_TASK(WaitResetComboEventTask);
	END_ABILITY_TASK(WaitCancelAttackEventTask);

	//HitDetectionSetter 언바인딩
	HitDetectionSetter.UnBind();

	//상태 초기화
	CurrentState = EAttackSequenceState::Idle;
	CurrentChargeProgress = EChargeProgress::NoCharge;

	CurrentAttackType = EAttackType::None;
	PreviousAttackType = EAttackType::None;
	CurrentAttackTags.Reset();

	ComboCounter = 0;
	MaxComboCount = 0;
	CurrentAttackData = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	DEBUG_LOG(TEXT("AttackSequenceAbility Ended - Cancelled: %s"), bWasCancelled ? TEXT("true") : TEXT("false"));
}