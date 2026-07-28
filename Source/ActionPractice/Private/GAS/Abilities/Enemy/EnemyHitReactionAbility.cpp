#include "GAS/Abilities/Enemy/EnemyHitReactionAbility.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyHitReactionAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyHitReactionAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Constructor + Initialization"
UEnemyHitReactionAbility::UEnemyHitReactionAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UEnemyHitReactionAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	StateHitReactionTag = UGameplayTagsSubsystem::GetStateHitReactionTag();

	//피격 리액션 몽타주는 EnemyDataAsset의 Combat 번들(AEnemyCharacter::BeginPlay 로드)로 이미 프리로드된다
	//사용 시점(SetMontageToPlayTask)에서 .Get() + 동기 폴백으로 읽으므로 어빌리티 개별 프리로드는 불필요
}
#pragma endregion

#pragma region "Ability Lifecycle"
void UEnemyHitReactionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//상태 태그 추가 (StateTree 전이 트리거)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateHitReactionTag.IsValid())
	{
		ASC->AddLooseGameplayTag(StateHitReactionTag);
	}

	//Poise 음수값으로 리액션 레벨 결정
	if (TriggerEventData)
	{
		const float PoiseValue = TriggerEventData->EventMagnitude;
		ReactionProcessor.SelectReactionLevel(PoiseValue);
		DEBUG_LOG(TEXT("EnemyHitReaction activated with Poise=%.1f, Level=%d"), PoiseValue, (int32)ReactionProcessor.GetReactionLevel());
	}

	StartMontageWithEventsTask();
}

void UEnemyHitReactionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//상태 태그 제거
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateHitReactionTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(StateHitReactionTag);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#pragma endregion

#pragma region "IMontageAbilityInterface"
UAnimMontage* UEnemyHitReactionAbility::SetMontageToPlayTask()
{
	//리액션 레벨에 해당하는 몽타주를 EnemyDataAsset(Combat 번들 프리로드분)에서 .Get(), 미완료 시 동기 폴백
	const AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	const UEnemyDataAsset* EnemyData = Enemy ? Enemy->GetEnemyData() : nullptr;
	if (!EnemyData)
	{
		return nullptr;
	}

	const TSoftObjectPtr<UAnimMontage>* SoftMontage = nullptr;
	const TCHAR* LevelName = TEXT("Light");

	switch (ReactionProcessor.GetReactionLevel())
	{
	case EReactionLevel::Heavy:  SoftMontage = &EnemyData->HitReactionHeavyMontage;  LevelName = TEXT("Heavy");  break;
	case EReactionLevel::Middle: SoftMontage = &EnemyData->HitReactionMiddleMontage; LevelName = TEXT("Middle"); break;
	case EReactionLevel::Light:
	default:                     SoftMontage = &EnemyData->HitReactionLightMontage;  LevelName = TEXT("Light");  break;
	}

	UAnimMontage* Montage = SoftMontage->Get();

	if (!Montage && !SoftMontage->IsNull())
	{
		DEBUG_LOG(TEXT("[AsyncPreload] HitReaction %s montage not preloaded, sync fallback: %s"), LevelName, *SoftMontage->ToString());
		Montage = SoftMontage->LoadSynchronous();
	}

	return Montage;
}

void UEnemyHitReactionAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("SetUpPlayMontageWithEventsTask: No task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UEnemyHitReactionAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UEnemyHitReactionAbility::OnTaskMontageInterrupted);
}

void UEnemyHitReactionAbility::StartMontageWithEventsTask()
{
	//폴백 로그는 SetMontageToPlayTask 내부([AsyncPreload])에서 일원화 처리된다
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("No HitReaction montage"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
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

	DEBUG_LOG(TEXT("EnemyHitReaction montage started: %s"), *MontageToPlay->GetName());
}

void UEnemyHitReactionAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("EnemyHitReaction montage completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEnemyHitReactionAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("EnemyHitReaction montage interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
#pragma endregion
