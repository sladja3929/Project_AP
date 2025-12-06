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
	DamageReductionMultiplier = 0.5f;
	StaminaDamageReduction = 0.5f;
	BlockAngle = 120.0f;
	ParryWindow = 0.3f;
}

void UBlockAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	PlayAction();
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
	
	bCreateTask = true;
}

void UBlockAbility::PlayAction()
{
	
	ExecuteMontageTask();
}

UAnimMontage* UBlockAbility::SetMontageToPlayTask()
{
	if (!WeaponBlockData)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No WeaponBlockData"));
		return nullptr;
	}

	// 소프트 레퍼런스를 실제 오브젝트로 로드
	UAnimMontage* Montage = WeaponBlockData->BlockIdleMontage.LoadSynchronous();
	if (!Montage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load BlockIdleMontage"));
		return nullptr;
	}

	return Montage;
}

void UBlockAbility::BindEventsAndReadyMontageTask()
{
	
}

void UBlockAbility::ExecuteMontageTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("No Montage to Play"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
    
	if (bCreateTask) // 커스텀 태스크 생성
	{        
		PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
			this,
			NAME_None,
			MontageToPlay,
			1.0f,
			NAME_None,
			1.0f
		);
		
		if (PlayMontageWithEventsTask)
		{        
			// 델리게이트 바인딩 - 사용하지 않는 델리게이트도 있음
			PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UBlockAbility::OnTaskMontageCompleted);
			PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UBlockAbility::OnTaskMontageInterrupted);

			// 태스크 활성화
			PlayMontageWithEventsTask->ReadyForActivation();
		}
    
		else
		{
			DEBUG_LOG(TEXT("No Montage Task"));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
	}

	else //태스크 중간에 몽타주 바꾸기
	{
		PlayMontageWithEventsTask->ChangeMontageAndPlay(MontageToPlay);
	}
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


void UBlockAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	DEBUG_LOG(TEXT("Block Input Released - End Ability"));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UBlockAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	DEBUG_LOG(TEXT("Block cancel"));
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UBlockAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bCreateTask = false;
	
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (PlayMontageWithEventsTask)
		{
			PlayMontageWithEventsTask->bStopMontageWhenAbilityCancelled = bWasCancelled;
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}