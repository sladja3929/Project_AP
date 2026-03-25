#include "GAS/Abilities/Player/ParryAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"

#define ENABLE_DEBUG_LOG 1

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
		return BlockData->ParryMontage.LoadSynchronous();
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
			ASC->AddLooseGameplayTag(StateParryingTag);
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
	if (ASC && StateParryingTag.IsValid())
	{
		while (ASC->HasMatchingGameplayTag(StateParryingTag))
		{
			ASC->RemoveLooseGameplayTag(StateParryingTag);
		}
	}
}
#pragma endregion
