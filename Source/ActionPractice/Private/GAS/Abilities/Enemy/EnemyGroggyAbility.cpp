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

	//그로기 몽타주는 EnemyDataAsset의 Combat 번들(AEnemyCharacter::BeginPlay 로드)로 이미 프리로드된다
	//사용 시점(SetMontageToPlayTask)에서 .Get() + 동기 폴백으로 읽으므로 어빌리티 개별 프리로드는 불필요
	//루프 지속 시간(설정값)만 캐싱한다
	if (const AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo(ActorInfo))
	{
		if (const UEnemyDataAsset* EnemyData = Enemy->GetEnemyData())
		{
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
	//현재 phase에 해당하는 몽타주를 EnemyDataAsset(Combat 번들 프리로드분)에서 .Get(), 미완료 시 동기 폴백
	const AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	const UEnemyDataAsset* EnemyData = Enemy ? Enemy->GetEnemyData() : nullptr;
	if (!EnemyData)
	{
		return nullptr;
	}

	const TSoftObjectPtr<UAnimMontage>* SoftMontage = nullptr;
	const TCHAR* PhaseName = TEXT("None");

	switch (CurrentPhase)
	{
	case EGroggyPhase::Start: SoftMontage = &EnemyData->GroggyStartMontage; PhaseName = TEXT("Start"); break;
	case EGroggyPhase::Loop:  SoftMontage = &EnemyData->GroggyLoopMontage;  PhaseName = TEXT("Loop");  break;
	case EGroggyPhase::End:   SoftMontage = &EnemyData->GroggyEndMontage;   PhaseName = TEXT("End");   break;
	default: return nullptr;
	}

	UAnimMontage* Montage = SoftMontage->Get();

	if (!Montage && !SoftMontage->IsNull())
	{
		DEBUG_LOG(TEXT("[AsyncPreload] Groggy %s montage not preloaded, sync fallback: %s"), PhaseName, *SoftMontage->ToString());
		Montage = SoftMontage->LoadSynchronous();
	}

	return Montage;
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
	//폴백 로그는 SetMontageToPlayTask 내부([AsyncPreload])에서 일원화 처리된다
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

	//루프 몽타주 지정 여부는 EnemyDataAsset 소프트 포인터로 판별 (Combat 번들 프리로드분)
	//지정돼 있으면 몽타주 재생, 없으면 타이머만으로 대기 (기존 동작 유지)
	bool bHasLoopMontage = false;
	if (const AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo())
	{
		if (const UEnemyDataAsset* EnemyData = Enemy->GetEnemyData())
		{
			bHasLoopMontage = !EnemyData->GroggyLoopMontage.IsNull();
		}
	}

	if (bHasLoopMontage)
	{
		//루프 몽타주 재생 (SetMontageToPlayTask가 .Get()+동기 폴백으로 획득)
		StartMontageWithEventsTask();

		//몽타주 로드가 완전히 실패하면 StartMontageWithEventsTask 내부에서 이미 EndAbility된다
		//그 경우 타이머를 걸면 종료된(InstancedPerActor 재사용) 인스턴스에 좀비 타이머가 남으므로 스킵
		if (!IsActive())
		{
			return;
		}
	}

	//루프 지속 시간 후 End Phase로 전환하는 타이머 (몽타주 유무와 무관하게 설정)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GroggyLoopTimerHandle,
			this,
			&UEnemyGroggyAbility::OnGroggyLoopTimerExpired,
			CachedGroggyLoopDuration,
			false
		);
		DEBUG_LOG(TEXT("Groggy loop timer set: %.1f seconds (montage=%d)"), CachedGroggyLoopDuration, bHasLoopMontage ? 1 : 0);
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

