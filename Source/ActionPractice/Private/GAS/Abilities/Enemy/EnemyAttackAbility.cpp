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

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEnemyAttackAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EventNotifyRotateToTargetTag = UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTag();
	EventNotifyAddComboTag = UGameplayTagsSubsystem::GetEventNotifyAddComboTag();

	if (!EventNotifyRotateToTargetTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyRotateToTargetTag is not valid"));
	}

	if (!EventNotifyAddComboTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyAddComboTag is not valid"));
	}
}

void UEnemyAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ComboCounter = -1;
	PlayAction();
}

void UEnemyAttackAbility::SetHitDetectionConfig()
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("No Character"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//HitDetectionSetter 초기화
	if (!HitDetectionSetter.Init(BossCharacter->GetHitDetectionInterface()))
	{
		DEBUG_LOG(TEXT("Failed to init HitDetectionSetter"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//HitDetectionSetter 바인딩
	if (!HitDetectionSetter.Bind(this))
	{
		DEBUG_LOG(TEXT("Failed to bind HitDetectionSetter"));
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

void UEnemyAttackAbility::PlayAction()
{
	SetHitDetectionConfig();

	ExecuteMontageTask();
}

UAnimMontage* UEnemyAttackAbility::SetMontageToPlayTask()
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No BossCharacter"));
		return nullptr;
	}

	const UEnemyDataAsset* EnemyData = BossCharacter->GetEnemyData();
	if (!EnemyData)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No EnemyData"));
		return nullptr;
	}

	const FNamedAttackData* AttackData = EnemyData->NamedAttackData.Find(AttackName);
	if (!AttackData)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: Attack data not found for name: %s"), *AttackName.ToString());
		return nullptr;
	}

	DEBUG_LOG(TEXT("SetMontageToPlayTask: Found montage for attack: %s"), *AttackName.ToString());
	return AttackData->AttackMontage.Get();
}

void UEnemyAttackAbility::ExecuteMontageTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("No Montage to Play"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 커스텀 태스크 생성
	PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		1.0f
	);

	BindEventsAndReadyMontageTask();
}

void UEnemyAttackAbility::BindEventsAndReadyMontageTask()
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
	PlayMontageWithEventsTask->OnNotifyEventsReceived.AddDynamic(this, &UEnemyAttackAbility::OnTaskNotifyEventsReceived);

	//노티파이 이벤트 바인딩
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyRotateToTargetTag);
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyAddComboTag);

	//태스크 활성화
	PlayMontageWithEventsTask->ReadyForActivation();
}

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

void UEnemyAttackAbility::OnTaskNotifyEventsReceived(FGameplayEventData Payload)
{
	if (Payload.EventTag == EventNotifyRotateToTargetTag)
	{
		OnEventRotateToTarget(Payload);
	}
	else if (Payload.EventTag == EventNotifyAddComboTag)
	{
		OnEventAddCombo(Payload);
	}
}

void UEnemyAttackAbility::OnEventRotateToTarget(FGameplayEventData Payload)
{
	ABossCharacter* BossCharacter = GetBossCharacterFromActorInfo();
	if (!BossCharacter)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No BossCharacter"));
		return;
	}

	//AI Controller에서 타겟 가져오기
	AEnemyAIController* AIController = GetEnemyAIControllerFromActorInfo();
	if (!AIController)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No AIController"));
		return;
	}

	//DetectedPlayer 가져오기
	AActionPracticeCharacter* TargetPlayer = AIController->GetDetectedPlayer();
	if (!TargetPlayer)
	{
		DEBUG_LOG(TEXT("OnEventRotateToTarget: No DetectedPlayer"));
		return;
	}

	//타겟을 향해 회전
	BossCharacter->RotateToTarget(TargetPlayer, RotateTime);
	DEBUG_LOG(TEXT("OnEventRotateToTarget: Rotating to %s"), *TargetPlayer->GetName());
}

void UEnemyAttackAbility::OnEventAddCombo(FGameplayEventData Payload)
{
	ComboCounter++;

	DEBUG_LOG(TEXT("OnEventAddCombo: ComboCounter incremented to %d"), ComboCounter);

	//PrepareHitDetection으로 공격 데이터 갱신
	if (!HitDetectionSetter.PrepareHitDetection(AttackName, ComboCounter))
	{
		DEBUG_LOG(TEXT("OnEventAddCombo: Failed to prepare HitDetection for combo %d"), ComboCounter);
		return;
	}

	DEBUG_LOG(TEXT("OnEventAddCombo: HitDetection prepared for combo %d"), ComboCounter);
}

void UEnemyAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	DEBUG_LOG(TEXT("EndAbility %d"), bWasCancelled);

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