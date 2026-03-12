#include "GAS/Abilities/Player/PlayerDeathAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Games/ActionPracticePlayerController.h"
#include "Games/ActionPracticeGameMode.h"
#include "Interaction/Bonfire.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Animation/AnimInstance.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogPlayerDeathAbility, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogPlayerDeathAbility, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UPlayerDeathAbility::UPlayerDeathAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UPlayerDeathAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	AbilityDeathTag = UGameplayTagsSubsystem::GetAbilityDeathTag();
	StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();

	if (!AbilityDeathTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityDeathTag is not valid"));
	}
	if (!StateDeadTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateDeadTag is not valid"));
	}
}

void UPlayerDeathAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//다른 일반 어빌리티 취소 (자기 자신 제외)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CancelAllAbilities(this);
	}

	//이동 비활성화
	DisableCharacterMovement();

	//데스 몽타주 재생 또는 바로 딜레이 단계로
	//StateDead 태그는 몽타주 종료 시 AddStateDeadTag()에서 추가 (UI 타이밍 분리)
	if (DeathMontage)
	{
		StartMontageWithEventsTask();
	}
	
	else
	{
		//몽타주 없을 때는 즉시 StateDead 추가 후 딜레이
		AddStateDeadTag();
		StartRespawnDelay();
	}
}

#pragma region "Montage Settings"

UAnimMontage* UPlayerDeathAbility::SetMontageToPlayTask()
{
	return DeathMontage;
}

void UPlayerDeathAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("SetUpPlayMontageWithEventsTask: No task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UPlayerDeathAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UPlayerDeathAbility::OnTaskMontageInterrupted);
}

void UPlayerDeathAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("StartMontageWithEventsTask: No death montage, skipping to delay"));
		StartRespawnDelay();
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
}

void UPlayerDeathAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("OnTaskMontageCompleted: holding last pose"));

	//마지막 포즈 유지 (몽타주 에셋의 BlendOut을 0으로 설정해야 효과적)
	if (ABaseCharacter* Character = GetBaseCharacterFromActorInfo())
	{
		if (UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_Pause(DeathMontage);
		}
	}

	//StateDead 추가 → PlayerController가 감지하여 UI 표시
	AddStateDeadTag();

	StartRespawnDelay();
}

void UPlayerDeathAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("OnTaskMontageInterrupted"));

	//인터럽트 경로에서도 StateDead 추가
	AddStateDeadTag();

	StartRespawnDelay();
}

#pragma endregion

#pragma region "Flow Control"

void UPlayerDeathAbility::StartRespawnDelay()
{
	END_ABILITY_TASK(WaitDelayTask);

	WaitDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RespawnDelay);
	if (WaitDelayTask)
	{
		WaitDelayTask->OnFinish.AddDynamic(this, &UPlayerDeathAbility::OnRespawnDelayFinished);
		WaitDelayTask->ReadyForActivation();
		DEBUG_LOG(TEXT("StartRespawnDelay: %.1f sec"), RespawnDelay);
	}
	else
	{
		PerformRespawn();
	}
}

void UPlayerDeathAbility::OnRespawnDelayFinished()
{
	PerformRespawn();
}

void UPlayerDeathAbility::PerformRespawn()
{
	DEBUG_LOG(TEXT("PerformRespawn"));

	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	//LastActivatedBonfire 기준 위치로 복귀
	AActionPracticePlayerController* PC = Cast<AActionPracticePlayerController>(Character->GetController());
	if (PC)
	{
		if (ABonfire* Bonfire = PC->GetLastActivatedBonfire())
		{
			const FTransform RespawnTransform = Bonfire->GetRespawnTransform();
			Character->SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
			DEBUG_LOG(TEXT("PerformRespawn: Teleported to Bonfire %s"), *GetNameSafe(Bonfire));
		}
		else
		{
			DEBUG_LOG(TEXT("PerformRespawn: No LastActivatedBonfire, keeping current location"));
		}
	}

	//회복 GE 적용
	ApplyRespawnRecovery();

	//적 리셋
	RequestEnemyReset();

	//이동 복구
	RestoreCharacterMovement();

	//StateDead 태그 제거 (BaseASC::HandleDeath에서 추가된 것)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && StateDeadTag.IsValid())
	{
		while (ASC->HasMatchingGameplayTag(StateDeadTag))
		{
			ASC->RemoveLooseGameplayTag(StateDeadTag);
			ASC->RemoveMinimalReplicationGameplayTag(StateDeadTag);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

#pragma endregion

#pragma region "Helper"

void UPlayerDeathAbility::AddStateDeadTag()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StateDeadTag.IsValid()) return;

	ASC->AddLooseGameplayTag(StateDeadTag);
	ASC->AddMinimalReplicationGameplayTag(StateDeadTag);
	DEBUG_LOG(TEXT("AddStateDeadTag: StateDead added"));
}

void UPlayerDeathAbility::DisableCharacterMovement()
{
	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character) return;

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp) return;

	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
	DEBUG_LOG(TEXT("DisableCharacterMovement"));
}

void UPlayerDeathAbility::RestoreCharacterMovement()
{
	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character) return;

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp) return;

	if (MoveComp->MovementMode == MOVE_None)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		DEBUG_LOG(TEXT("RestoreCharacterMovement: Walking mode restored"));
	}
}

void UPlayerDeathAbility::ApplyRespawnRecovery()
{
	if (!RespawnRecoveryEffect)
	{
		DEBUG_LOG(TEXT("ApplyRespawnRecovery: RespawnRecoveryEffect not set"));
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	const float Level = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(RespawnRecoveryEffect, Level, ASC->MakeEffectContext());
	if (!Spec.IsValid()) return;

	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	DEBUG_LOG(TEXT("ApplyRespawnRecovery: GE applied"));
}

void UPlayerDeathAbility::RequestEnemyReset()
{
	UWorld* World = GetWorld();
	if (!World) return;

	AActionPracticeGameMode* GameMode = Cast<AActionPracticeGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	GameMode->ResetAllEnemies();
	DEBUG_LOG(TEXT("RequestEnemyReset: ResetAllEnemies called"));
}

#pragma endregion

void UPlayerDeathAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	//태스크 정리
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}
	END_ABILITY_TASK(WaitDelayTask);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
