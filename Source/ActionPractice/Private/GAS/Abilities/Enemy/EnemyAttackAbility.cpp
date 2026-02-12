#include "GAS/Abilities/Enemy/EnemyAttackAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/BossCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "GAS/AbilitySystemComponent/BossAbilitySystemComponent.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Items/AttackData.h"
#include "AI/EnemyAIController.h"
#include "Characters/ActionPracticeCharacter.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

//커브 이름 상수 정의
const FName UEnemyAttackAbility::CurveName_ActionRecovery = TEXT("ActionRecovery");

UEnemyAttackAbility::UEnemyAttackAbility()
{
	//보스는 서버 전용 (클라이언트는 애니메이션만 복제로 재생)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UEnemyAttackAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	CacheGameplayTags();
}

void UEnemyAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	DEBUG_LOG(TEXT("EnemyAttackAbility Activated"));

	ExecuteAttack();
}

#pragma region "Activate Initialization Functions"

void UEnemyAttackAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	//보스 데이터 캐싱
	CacheBossData();

	//HitDetectionSetter 바인딩
	BindHitDetectionSetter();

	//콤보 상태 초기화
	ComboCounter = 0;
	bPerformNextCombo = false;
}

void UEnemyAttackAbility::CacheGameplayTags()
{
	//이벤트 태그
	EventNotifyRotateToTargetTag = UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTag();
	EventNotifyCheckConditionTag = UGameplayTagsSubsystem::GetEventNotifyCheckConditionTag();

	//태그 유효성 검사
	if (!EventNotifyRotateToTargetTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyRotateToTargetTag is not valid"));
	}
	if (!EventNotifyCheckConditionTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyCheckConditionTag is not valid"));
	}
}

void UEnemyAttackAbility::CacheBossData()
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("CacheBossData: BossCharacter is nullptr. Ability=%s"), *GetName());
		return;
	}

	const UEnemyDataAsset* EnemyData = BossCharacter->GetEnemyData();
	if (!EnemyData)
	{
		DEBUG_LOG(TEXT("CacheBossData: EnemyData is nullptr. Ability=%s"), *GetName());
		return;
	}

	EnemyAttackData = EnemyData->NamedAttackData.Find(AttackName);
	if (!EnemyAttackData || EnemyAttackData->ComboSequence.Num() == 0)
	{
		DEBUG_LOG(TEXT("CacheBossData: Attack data not found for name: %s"), *AttackName.ToString());
		return;
	}

	MaxComboCount = EnemyAttackData->ComboSequence.Num();

	//Ability 시작 시 AIController로부터 CurrentTarget 정보 캐싱
	AEnemyAIController* AIController = GetEnemyAIControllerFromActorInfo();
	if (AIController)
	{
		CachedTargetInfo = AIController->GetCurrentTarget();
	}

	DEBUG_LOG(TEXT("CacheBossData: Cached AttackName=%s, MaxComboCount=%d"), *AttackName.ToString(), MaxComboCount);
}

void UEnemyAttackAbility::BindHitDetectionSetter()
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("BindHitDetectionSetter: No BossCharacter"));
		return;
	}

	//HitDetectionSetter 초기화
	if (!HitDetectionSetter.Init(BossCharacter->GetHitDetectionInterface()))
	{
		DEBUG_LOG(TEXT("BindHitDetectionSetter: Failed to init HitDetectionSetter"));
		return;
	}

	//HitDetectionSetter 바인딩
	if (!HitDetectionSetter.Bind(this))
	{
		DEBUG_LOG(TEXT("BindHitDetectionSetter: Failed to bind HitDetectionSetter"));
		return;
	}
}

#pragma endregion

#pragma region "Execute Logic"

void UEnemyAttackAbility::ExecuteAttack()
{
	//데이터 검증
	if (!EnemyAttackData || EnemyAttackData->ComboSequence.Num() == 0)
	{
		DEBUG_LOG(TEXT("ExecuteAttack: No valid EnemyAttackData"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	SetHitDetectionConfig();

	START_WAIT_EVENT_TASK(WaitRotateToTargetEventTask, EventNotifyRotateToTargetTag, OnEventRotateToTarget, nullptr, true, true);
	START_WAIT_EVENT_TASK(WaitCheckConditionEventTask, EventNotifyCheckConditionTag, OnEventCheckCondition, nullptr, true, true);
	StartMontageWithEventsTask();
}

#pragma endregion

#pragma region "Hit Detection"

void UEnemyAttackAbility::SetHitDetectionConfig()
{
	//PrepareHitDetection 호출
	if (!HitDetectionSetter.PrepareHitDetection(AttackName, ComboCounter))
	{
		DEBUG_LOG(TEXT("Failed to prepare HitDetection"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
}

void UEnemyAttackAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
	//Source ASC (공격자, AttackAbility 소유자)
	UBossAbilitySystemComponent* SourceASC = GetBossAbilitySystemComponentFromActorInfo();
	if (!HitActor || !SourceASC) return;

	//Target ASC (피격자)
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC) return;

	//Source ASC에서 GE Spec 생성
	FGameplayEffectSpecHandle SpecHandle = SourceASC->CreateAttackGameplayEffectSpec(DamageInstantEffect, GetAbilityLevel(), this, AttackData);

	if (SpecHandle.IsValid())
	{
		//Target에게 적용
		FActiveGameplayEffectHandle ActiveGEHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		//적용에 성공했으면
		if (ActiveGEHandle.WasSuccessfullyApplied())
		{
			DEBUG_LOG(TEXT("Damage Applied Successfully"));
		}
	}
}

#pragma endregion

#pragma region "Task Functions"

UAnimMontage* UEnemyAttackAbility::SetMontageToPlayTask()
{
	if (!EnemyAttackData)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No EnemyAttackData"));
		return nullptr;
	}

	if (ComboCounter < 0) ComboCounter = 0;
	
	if (ComboCounter >= MaxComboCount)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: ComboCounter out of range. ComboCounter=%d, MaxComboCount=%d"), ComboCounter, MaxComboCount);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return nullptr;
	}

	//소프트 레퍼런스를 실제 오브젝트로 로드
	const auto& ComboData = EnemyAttackData->ComboSequence[ComboCounter];
	UAnimMontage* Montage = ComboData.AttackMontage.LoadSynchronous();
	if (!Montage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load montage. AttackName=%s, ComboIndex=%d"),
			*AttackName.ToString(), ComboCounter);
		return nullptr;
	}

	return Montage;
}

void UEnemyAttackAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("No Montage to Play"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	//기존 태스크가 존재하면
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}
	
	//커스텀 태스크 생성
	PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		1.0f
	);
    
	SetUpPlayMontageWithEventsTask();

	//태스크 활성화
	PlayMontageWithEventsTask->ReadyForActivation();
}

void UEnemyAttackAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//커스텀 몽타주 태스크 델리게이트 바인딩
	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UEnemyAttackAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UEnemyAttackAbility::OnTaskMontageInterrupted);

	//=== 커브 폴링 활성화 (ActionRecovery 노티파이 대체) ===
	PlayMontageWithEventsTask->EnableCurvePolling(CurveName_ActionRecovery);

	//커브 에지 델리게이트 바인딩
	PlayMontageWithEventsTask->OnCurveRisingEdge.AddDynamic(this, &UEnemyAttackAbility::OnCurveRisingEdgeReceived);
	PlayMontageWithEventsTask->OnCurveFallingEdge.AddDynamic(this, &UEnemyAttackAbility::OnCurveFallingEdgeReceived);

	//태스크 활성화
	PlayMontageWithEventsTask->ReadyForActivation();
}

#pragma endregion

#pragma region "Handler Functions"

void UEnemyAttackAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("Montage Task Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEnemyAttackAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Montage Task Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UEnemyAttackAbility::OnEventRotateToTarget(FGameplayEventData Payload)
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No BossCharacter"));
		return;
	}

	//캐싱된 Target Actor 가져오기
	if (!CachedTargetInfo.IsValid())
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No Cached Target"));
		return;
	}

	AActor* TargetActor = CachedTargetInfo.Actor.Get();
	if (!TargetActor)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: Target Actor is nullptr"));
		return;
	}

	//타겟을 향해 회전
	BossCharacter->RotateToTarget(TargetActor, RotateTime);
	DEBUG_LOG(TEXT("OnEventRotateToTarget: Rotating to %s"),	*TargetActor->GetName());
}

void UEnemyAttackAbility::OnEventCheckCondition(FGameplayEventData Payload)
{	
	AEnemyAIController* AIController = GetEnemyAIControllerFromActorInfo();
	if (!AIController)
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: No AIController"));
		bPerformNextCombo = false;
		return;
	}

	FCurrentTarget CurrentTargetInfo = AIController->GetCurrentTarget();
	if (!CurrentTargetInfo.IsValid())
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: No Valid Target"));
		bPerformNextCombo = false;
		return;
	}

	//거리 체크
	if (CurrentTargetInfo.Distance > MaxTargetDistance)
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: Target too far - Distance: %.2f, Max: %.2f"), CurrentTargetInfo.Distance, MaxTargetDistance);
		bPerformNextCombo = false;
		return;
	}

	//각도 체크 (절대값)
	if (FMath::Abs(CurrentTargetInfo.AngleToTarget) > MaxTargetAngle)
	{
		DEBUG_LOG(TEXT("OnEventCheckCondition: Target angle out of range - Angle: %.2f, Max: %.2f"), CurrentTargetInfo.AngleToTarget, MaxTargetAngle);
		bPerformNextCombo = false;
		return;
	}
	
	//해당 노티파이가 존재해야 다음 콤보 진행 가능
	bPerformNextCombo = true;
	ComboCounter++;
	DEBUG_LOG(TEXT("OnEventCheckCondition: Passed - Distance: %.2f, Angle: %.2f"), CurrentTargetInfo.Distance, CurrentTargetInfo.AngleToTarget);
}

void UEnemyAttackAbility::OnCurveRisingEdgeReceived(FName CurveName)
{
	DEBUG_LOG(TEXT("Curve Rising Edge: %s"), *CurveName.ToString());

	//ActionRecovery 상승 에지는 현재 특별한 처리 없음
}

void UEnemyAttackAbility::OnCurveFallingEdgeReceived(FName CurveName)
{
	DEBUG_LOG(TEXT("Curve Falling Edge: %s"), *CurveName.ToString());

	if (CurveName == CurveName_ActionRecovery)
	{
		OnActionRecoveryEnd();
	}
}

void UEnemyAttackAbility::OnActionRecoveryEnd()
{
	DEBUG_LOG(TEXT("OnActionRecoveryEnd: bPerformNextCombo=%s"), bPerformNextCombo ? TEXT("true") : TEXT("false"));

	if (bPerformNextCombo)
	{
		DEBUG_LOG(TEXT("OnActionRecoveryEnd: Performing Next Combo"));
		ExecuteAttack();
	}
}

#pragma endregion

void UEnemyAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	DEBUG_LOG(TEXT("EndAbility %d"), bWasCancelled);

	ComboCounter = 0;
	bPerformNextCombo = false;
	
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		//HitDetectionSetter 언바인딩
		HitDetectionSetter.UnBind();

		if (PlayMontageWithEventsTask)
		{
			PlayMontageWithEventsTask->bStopMontageWhenAbilityCancelled = bWasCancelled;
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}