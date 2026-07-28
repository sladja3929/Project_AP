#include "GAS/Abilities/Player/BlockAbility.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "Items/WeaponDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBlockAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBlockAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UBlockAbility::UBlockAbility()
{
	PlayMontageWithEventsTask = nullptr;
}

void UBlockAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!WeaponBlockData)
	{
		return;
	}

	StartMontageWithEventsTask();
	StartWaitInputReleaseTask(true);
}

void UBlockAbility::ActivateInitSettings()
{
	WeaponBlockData = FWeaponAbilityStatics::GetBlockDataFromAbility(this);
	if (!WeaponBlockData)
	{
		DEBUG_LOG(TEXT("Cannot Load Block Data"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
}

UAnimMontage* UBlockAbility::SetMontageToPlayTask()
{
	if (!WeaponBlockData)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No WeaponBlockData"));
		return nullptr;
	}

	//프리로드 완료분은 .Get()으로 즉시 획득(사실상 no-op), 미완료 시에만 동기 폴백
	//폴백은 비동기 전환 실패가 아니라 입력 반응성 + 데디서버 코옵 결정론을 위해 의도적으로 남긴 안전망이다
	UAnimMontage* Montage = WeaponBlockData->BlockIdleMontage.Get();

	if (!Montage && !WeaponBlockData->BlockIdleMontage.IsNull())
	{
		DEBUG_LOG(TEXT("[AsyncPreload] Block BlockIdleMontage not preloaded, sync fallback: %s"), *WeaponBlockData->BlockIdleMontage.ToString());
		Montage = WeaponBlockData->BlockIdleMontage.LoadSynchronous();
	}

	if (!Montage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load BlockIdleMontage"));
		return nullptr;
	}

	return Montage;
}

void UBlockAbility::SetUpPlayMontageWithEventsTask()
{
	
}

void UBlockAbility::StartMontageWithEventsTask()
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

void UBlockAbility::OnTaskMontageCompleted()
{
	//Idle은 실행 X, 오직 Reaction만
	DEBUG_LOG(TEXT("Montage Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBlockAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Montage Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBlockAbility::OnWaitInputRelease(float TimeHeld)
{
	DEBUG_LOG(TEXT("Block WaitInputRelease - End Ability (TimeHeld: %.3f)"), TimeHeld);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UBlockAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	DEBUG_LOG(TEXT("Block cancel"));
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UBlockAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{	
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (PlayMontageWithEventsTask)
		{
			PlayMontageWithEventsTask->bStopMontageWhenAbilityCancelled = bWasCancelled;
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}