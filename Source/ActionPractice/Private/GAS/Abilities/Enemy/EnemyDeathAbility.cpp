#include "GAS/Abilities/Enemy/EnemyDeathAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "AI/EnemyAIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyDeathAbility, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyDeathAbility, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UEnemyDeathAbility::UEnemyDeathAbility()
{
	//적은 서버 전용 (클라이언트는 몽타주 복제로 재생)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UEnemyDeathAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();

	//EnemyDataAsset에서 사망 몽타주 캐싱
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo(ActorInfo);
	if (Enemy)
	{
		const UEnemyDataAsset* EnemyData = Enemy->GetEnemyData();
		if (EnemyData && !EnemyData->DeathMontage.IsNull())
		{
			CachedDeathMontage = EnemyData->DeathMontage.LoadSynchronous();
		}
	}

	if (!StateDeadTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateDeadTag is not valid"));
	}
}

void UEnemyDeathAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	DEBUG_LOG(TEXT("EnemyDeathAbility Activated"));

	//서버 전용 사망 처리 (어빌리티 취소, 이동 비활성, AI 정지)
	ExecuteDeathServerLogic();

	//사망 몽타주 재생 또는 즉시 사망 처리
	if (CachedDeathMontage)
	{
		StartMontageWithEventsTask();
	}
	else
	{
		//몽타주 없으면 즉시 사망 상태 완료
		FreezeAnimPose();
		AddStateDeadTag();
		HideHealthBar();
		DEBUG_LOG(TEXT("No death montage — immediate death state"));
	}
}

#pragma region "Activate Initialization"

void UEnemyDeathAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();
}

void UEnemyDeathAbility::ExecuteDeathServerLogic()
{
	//다른 어빌리티 취소 (자기 자신 제외)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CancelAllAbilities(this);
	}

	//이동 비활성화
	DisableCharacterMovement();

	//AI 로직 정지
	StopEnemyAI();

	//락온 해제 — 모든 클라이언트에서 이 적을 타깃으로 한 락온 해제
	if (AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo())
	{
		Enemy->Multicast_ReleaseLockOn();
	}
}

#pragma endregion

#pragma region "Montage Settings"

UAnimMontage* UEnemyDeathAbility::SetMontageToPlayTask()
{
	return CachedDeathMontage;
}

void UEnemyDeathAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("SetUpPlayMontageWithEventsTask: No task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayMontageWithEventsTask->OnMontageBlendOut.AddDynamic(this, &UEnemyDeathAbility::OnTaskMontageBlendOut);
	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UEnemyDeathAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UEnemyDeathAbility::OnTaskMontageInterrupted);
}

void UEnemyDeathAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("StartMontageWithEventsTask: No death montage, immediate death"));
		FreezeAnimPose();
		AddStateDeadTag();
		HideHealthBar();
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

	DEBUG_LOG(TEXT("Death montage started: %s"), *MontageToPlay->GetName());
}

void UEnemyDeathAbility::OnTaskMontageBlendOut()
{
	DEBUG_LOG(TEXT("OnTaskMontageBlendOut: freezing pose"));

	//포즈 고정
	FreezeAnimPose();

	//StateDead 태그 추가
	AddStateDeadTag();

	//HP바 숨김
	HideHealthBar();
}

void UEnemyDeathAbility::OnTaskMontageCompleted()
{
	//BlendOut에서 정상 처리됨 — 폴백
	DEBUG_LOG(TEXT("OnTaskMontageCompleted: fallback path"));

	if (!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(StateDeadTag))
	{
		FreezeAnimPose();
		AddStateDeadTag();
		HideHealthBar();
	}
}

void UEnemyDeathAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("OnTaskMontageInterrupted"));

	FreezeAnimPose();
	AddStateDeadTag();
	HideHealthBar();
}

#pragma endregion

#pragma region "Helpers"

void UEnemyDeathAbility::AddStateDeadTag()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StateDeadTag.IsValid()) return;

	ASC->AddLooseGameplayTag(StateDeadTag);
	ASC->AddMinimalReplicationGameplayTag(StateDeadTag);
	DEBUG_LOG(TEXT("AddStateDeadTag: StateDead added"));
}

void UEnemyDeathAbility::DisableCharacterMovement()
{
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy) return;

	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
	if (!MoveComp) return;

	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
	DEBUG_LOG(TEXT("DisableCharacterMovement"));
}

void UEnemyDeathAbility::StopEnemyAI()
{
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy) return;

	AEnemyAIController* AIController = Enemy->GetEnemyAIController();
	if (!AIController) return;

	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Death"));
		DEBUG_LOG(TEXT("StopEnemyAI: StateTree stopped"));
	}
}

void UEnemyDeathAbility::FreezeAnimPose()
{
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy) return;

	//bPauseAnims은 복제되지 않으므로 Multicast RPC로 모든 클라이언트에 전파
	Enemy->Multicast_FreezeAnimPose();
	DEBUG_LOG(TEXT("FreezeAnimPose: Multicast_FreezeAnimPose called"));
}

void UEnemyDeathAbility::HideHealthBar()
{
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy) return;

	Enemy->HideEnemyHealthBar();
	DEBUG_LOG(TEXT("HideHealthBar"));
}

#pragma endregion

void UEnemyDeathAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	DEBUG_LOG(TEXT("EnemyDeathAbility EndAbility (bWasCancelled=%s)"), bWasCancelled ? TEXT("true") : TEXT("false"));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
