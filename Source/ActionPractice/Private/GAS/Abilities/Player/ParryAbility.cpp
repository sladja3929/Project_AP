#include "GAS/Abilities/Player/ParryAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogParryAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogParryAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

const FName UParryAbility::CurveName_ParryWindow = TEXT("ParryWindow");

#pragma region "Constructor + Initialization"
UParryAbility::UParryAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UParryAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	StateParryingTag = UGameplayTagsSubsystem::GetStateParryingTag();
}
#pragma endregion

#pragma region "Ability Lifecycle"
void UParryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	DEBUG_LOG(TEXT("ParryAbility Activated"));

	if (!ConsumeStamina()) return;
	StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();
}

bool UParryAbility::ConsumeStamina()
{
	if (!ApplyStaminaCost())
	{
		DEBUG_LOG(TEXT("No Stamina"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return false;
	}

	return true;
}

void UParryAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//인터럽트/캔슬 시에도 안전하게 태그 정리
	CleanupParryingTag();

	DEBUG_LOG(TEXT("ParryAbility EndAbility, Cancelled=%s"), bWasCancelled ? TEXT("true") : TEXT("false"));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
#pragma endregion

#pragma region "IMontageAbilityInterface"
UAnimMontage* UParryAbility::SetMontageToPlayTask()
{
	const FBlockActionData* BlockData = FWeaponAbilityStatics::GetBlockDataFromAbility(this);
	if (BlockData && !BlockData->ParryMontage.IsNull())
	{
		DEBUG_LOG(TEXT("Playing ParryMontage from BlockData"));

		//프리로드 완료분은 .Get()으로 즉시 획득(사실상 no-op), 미완료 시에만 동기 폴백(의도적 결정론 안전망)
		UAnimMontage* Montage = BlockData->ParryMontage.Get();

		if (!Montage)
		{
			DEBUG_LOG(TEXT("[AsyncPreload] Parry ParryMontage not preloaded, sync fallback: %s"), *BlockData->ParryMontage.ToString());
			Montage = BlockData->ParryMontage.LoadSynchronous();
		}

		return Montage;
	}

	DEBUG_LOG(TEXT("No ParryMontage found in BlockData"));
	return nullptr;
}

void UParryAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//부모 설정 (몽타주 콜백 + EnableBufferInput/ActionRecovery 커브)
	Super::SetUpPlayMontageWithEventsTask();

	//ParryWindow 커브 추가 등록
	PlayMontageWithEventsTask->EnableCurvePolling(CurveName_ParryWindow);
}

void UParryAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("Parry Montage Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UParryAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Parry Montage Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
#pragma endregion

#pragma region "Curve Edge Handlers"
void UParryAbility::OnCurveRisingEdgeReceived(FName CurveName)
{
	//부모 커브 처리 (EnableBufferInput, ActionRecovery)
	Super::OnCurveRisingEdgeReceived(CurveName);

	if (CurveName == CurveName_ParryWindow)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC && StateParryingTag.IsValid())
		{
			AddParryingTag_Predicted();
			DEBUG_LOG(TEXT("ParryWindow OPEN - State.Parrying added"));
		}
	}
}

void UParryAbility::OnCurveFallingEdgeReceived(FName CurveName)
{
	//부모 커브 처리 (EnableBufferInput, ActionRecovery)
	Super::OnCurveFallingEdgeReceived(CurveName);

	if (CurveName == CurveName_ParryWindow)
	{
		CleanupParryingTag();
		DEBUG_LOG(TEXT("ParryWindow CLOSE - State.Parrying removed"));
	}
}
#pragma endregion

#pragma region "Helper"
void UParryAbility::CleanupParryingTag()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StateParryingTag.IsValid())
	{
		return;
	}

	const bool bIsAuthority = HasAuthority(&CurrentActivationInfo);

	while (ASC->HasMatchingGameplayTag(StateParryingTag))
	{
		ASC->RemoveLooseGameplayTag(StateParryingTag);

		//서버: 추가 시 사용한 복제 태그도 대칭 제거
		if (bIsAuthority)
		{
			ASC->RemoveMinimalReplicationGameplayTag(StateParryingTag);
		}
	}
}

void UParryAbility::AddParryingTag_Predicted()
{
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();

	if (!APASC || !StateParryingTag.IsValid())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const bool bIsAuthority = HasAuthority(&CurrentActivationInfo);
	const bool bIsLocallyControlled = ActorInfo && ActorInfo->IsLocallyControlled();

	//서버: 권위 태그 적용 + MinimalReplication 복제 (판정용 + 프록시 가시성)
	if (bIsAuthority)
	{
		APASC->AddLooseGameplayTag(StateParryingTag);
		APASC->AddMinimalReplicationGameplayTag(StateParryingTag);
		return;
	}

	//소유 클라(비권위): 예측 윈도우 안에서 태그 예측 적용
	if (bIsLocallyControlled)
	{
		FScopedPredictionWindow ScopedPrediction(APASC, true);

		APASC->AddLooseGameplayTag(StateParryingTag);
	}
}
#pragma endregion
