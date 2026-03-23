#include "GAS/Abilities/Enemy/EnemyGroggyAbility.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyGroggyAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyGroggyAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Constructor + Initialization"
UEnemyGroggyAbility::UEnemyGroggyAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UEnemyGroggyAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	StateGroggyTag = UGameplayTagsSubsystem::GetStateGroggyTag();

	//EnemyDataAsset에서 몽타주 및 설정 캐싱
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo(ActorInfo);
	if (Enemy)
	{
		const UEnemyDataAsset* EnemyData = Enemy->GetEnemyData();
		if (EnemyData)
		{
			if (!EnemyData->GroggyStartMontage.IsNull())
			{
				CachedGroggyStartMontage = EnemyData->GroggyStartMontage.LoadSynchronous();
			}
			if (!EnemyData->GroggyLoopMontage.IsNull())
			{
				CachedGroggyLoopMontage = EnemyData->GroggyLoopMontage.LoadSynchronous();
			}
			if (!EnemyData->GroggyEndMontage.IsNull())
			{
				CachedGroggyEndMontage = EnemyData->GroggyEndMontage.LoadSynchronous();
			}
			CachedGroggyLoopDuration = EnemyData->GroggyLoopDuration;
		}
	}
}
#pragma endregion

#pragma region "Ability Lifecycle"
void UEnemyGroggyAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//상태 태그 추가 (StateTree 전이 트리거)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateGroggyTag.IsValid())
	{
		ASC->AddLooseGameplayTag(StateGroggyTag);
	}

	DEBUG_LOG(TEXT("EnemyGroggy activated — starting Start phase"));

	//Start 단계 시작
	CurrentPhase = EGroggyPhase::Start;
	StartMontageWithEventsTask();
}

void UEnemyGroggyAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//타이머 정리
	if (GroggyLoopTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GroggyLoopTimerHandle);
		}
	}

	//상태 태그 제거
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateGroggyTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(StateGroggyTag);
	}

	CurrentPhase = EGroggyPhase::None;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
#pragma endregion

#pragma region "IMontageAbilityInterface"
UAnimMontage* UEnemyGroggyAbility::SetMontageToPlayTask()
{
	switch (CurrentPhase)
	{
	case EGroggyPhase::Start: return CachedGroggyStartMontage;
	case EGroggyPhase::Loop: return CachedGroggyLoopMontage;
	case EGroggyPhase::End: return CachedGroggyEndMontage;
	default: return nullptr;
	}
}

void UEnemyGroggyAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("SetUpPlayMontageWithEventsTask: No task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UEnemyGroggyAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UEnemyGroggyAbility::OnTaskMontageInterrupted);
}

void UEnemyGroggyAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("No montage for phase %d"), (int32)CurrentPhase);

		//몽타주 없으면 다음 Phase로 스킵
		if (CurrentPhase == EGroggyPhase::Start)
		{
			StartGroggyLoop();
		}
		else
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
		return;
	}

	//기존 태스크 정리
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}

	PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		1.0f
	);

	SetUpPlayMontageWithEventsTask();
	PlayMontageWithEventsTask->ReadyForActivation();

	DEBUG_LOG(TEXT("Groggy montage started: %s (Phase=%d)"), *MontageToPlay->GetName(), (int32)CurrentPhase);
}

void UEnemyGroggyAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("Groggy montage completed (Phase=%d)"), (int32)CurrentPhase);

	switch (CurrentPhase)
	{
	case EGroggyPhase::Start:
		StartGroggyLoop();
		break;

	case EGroggyPhase::Loop:
		//루프 몽타주가 자체 종료한 경우 — End로 전환
		StartGroggyEnd();
		break;

	case EGroggyPhase::End:
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		break;

	default:
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		break;
	}
}

void UEnemyGroggyAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Groggy montage interrupted (Phase=%d)"), (int32)CurrentPhase);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
#pragma endregion

#pragma region "Phase Transitions"
void UEnemyGroggyAbility::StartGroggyLoop()
{
	CurrentPhase = EGroggyPhase::Loop;

	if (CachedGroggyLoopMontage)
	{
		//루프 몽타주 재생
		StartMontageWithEventsTask();

		//루프 지속 시간 후 End Phase로 전환하는 타이머
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GroggyLoopTimerHandle,
				this,
				&UEnemyGroggyAbility::OnGroggyLoopTimerExpired,
				CachedGroggyLoopDuration,
				false
			);
			DEBUG_LOG(TEXT("Groggy loop timer set: %.1f seconds"), CachedGroggyLoopDuration);
		}
	}
	else
	{
		//루프 몽타주가 없으면 타이머만으로 대기
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GroggyLoopTimerHandle,
				this,
				&UEnemyGroggyAbility::OnGroggyLoopTimerExpired,
				CachedGroggyLoopDuration,
				false
			);
			DEBUG_LOG(TEXT("Groggy loop (no montage), timer: %.1f seconds"), CachedGroggyLoopDuration);
		}
	}
}

void UEnemyGroggyAbility::OnGroggyLoopTimerExpired()
{
	DEBUG_LOG(TEXT("Groggy loop timer expired — starting End phase"));
	StartGroggyEnd();
}

void UEnemyGroggyAbility::StartGroggyEnd()
{
	//루프 태스크 정리
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}

	CurrentPhase = EGroggyPhase::End;
	StartMontageWithEventsTask();
}
#pragma endregion

