#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "Input/InputBufferComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionRecoveryAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionRecoveryAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UActionRecoveryAbility::UActionRecoveryAbility()
{

}

void UActionRecoveryAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	ActionRecoveryStartTag = UGameplayTagsSubsystem::GetEventNotifyActionRecoveryStartTag();
	ActionRecoveryEndTag = UGameplayTagsSubsystem::GetEventNotifyActionRecoveryEndTag();
	EventInputByBufferTag = UGameplayTagsSubsystem::GetEventActionInputByBufferTag();
	EventPlayBufferTag = UGameplayTagsSubsystem::GetEventActionPlayBufferTag();
	StateRecoveringTag = UGameplayTagsSubsystem::GetStateRecoveringTag();

	if (!ActionRecoveryStartTag.IsValid())
	{
		DEBUG_LOG(TEXT("ActionRecoveryStartTag is not valid"));
	}
	if (!ActionRecoveryEndTag.IsValid())
	{
		DEBUG_LOG(TEXT("ActionRecoveryEndTag is not valid"));
	}
	if (!EventInputByBufferTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventInputByBufferTag is not valid"));
	}
	if (!EventPlayBufferTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventPlayBufferTag is not valid"));
	}
	if (!StateRecoveringTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateRecoveringTag is not valid"));
	}
}

void UActionRecoveryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	PlayAction();
}

void UActionRecoveryAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();
}

void UActionRecoveryAbility::AddStateRecoveringTag()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		DEBUG_LOG(TEXT("ActionRecoveryStart: No ASC"));
	}
	
	ASC->AddLooseGameplayTag(StateRecoveringTag);
	DEBUG_LOG(TEXT("Add State.Recovering"));
}

void UActionRecoveryAbility::RemoveStateRecoveringTags()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		DEBUG_LOG(TEXT("ActionRecoveryEnd: No ASC"));
	}

	//모든 StateRecovering 태그 제거 (스택된 태그 모두 제거)
	while (ASC->HasMatchingGameplayTag(StateRecoveringTag))
	{
		ASC->RemoveLooseGameplayTag(StateRecoveringTag);
	}

	DEBUG_LOG(TEXT("Remove All State.Recovering"));
}

bool UActionRecoveryAbility::ConsumeStamina()
{
	if (!ApplyStaminaCost())
	{
		DEBUG_LOG(TEXT("No Stamina"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return false;
	}

	return true;
}

bool UActionRecoveryAbility::RotateCharacter()
{
	if (!bRotateBeforeAction)
	{
		return false;
	}

	if (AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo())
	{
		DEBUG_LOG(TEXT("Rotating Character"));
		Character->RotateCharacterToInputDirection(RotateTime, bIgnoreLockOn);
		return true;
	}

	return false;
}

void UActionRecoveryAbility::PlayAction()
{
	if (!ConsumeStamina()) return;
	AddStateRecoveringTag();

	bool bShouldRotate = RotateCharacter();

	//캐릭터가 회전을 마칠 때까지 기다린 후에 몽타주 태스크 실행
	//회전하지 않으면 즉시 실행
	float DelayTime = bShouldRotate ? RotateTime : 0.0f;
	WaitDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, DelayTime);
	if (WaitDelayTask)
	{
		WaitDelayTask->OnFinish.AddDynamic(this, &UActionRecoveryAbility::ExecuteMontageTask);
		WaitDelayTask->ReadyForActivation();
	}
}

void UActionRecoveryAbility::ExecuteMontageTask()
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

void UActionRecoveryAbility::BindEventsAndReadyMontageTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}

	//커스텀 몽타주 태스크 델리게이트 바인딩
	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UActionRecoveryAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UActionRecoveryAbility::OnTaskMontageInterrupted);
	PlayMontageWithEventsTask->OnNotifyEventsReceived.AddDynamic(this, &UActionRecoveryAbility::OnTaskNotifyEventsReceived);

	//노티파이 이벤트 바인딩
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(ActionRecoveryStartTag);
	PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(ActionRecoveryEndTag);
	
	//태스크 활성화
	PlayMontageWithEventsTask->ReadyForActivation();
}
	
void UActionRecoveryAbility::ReadyInputByBufferTask()
{
	//이벤트 태스크 실행
	WaitInputByBufferEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EventInputByBufferTag, nullptr, false, true);
	
	WaitInputByBufferEventTask->EventReceived.AddDynamic(this, &UActionRecoveryAbility::OnEventInputByBuffer);
	
	WaitInputByBufferEventTask->ReadyForActivation();
}

void UActionRecoveryAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("Montage Task Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UActionRecoveryAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("Montage Task Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UActionRecoveryAbility::OnTaskNotifyEventsReceived(FGameplayEventData Payload)
{
	if (Payload.EventTag == ActionRecoveryStartTag) OnEventActionRecoveryStart(Payload);
	
	else if (Payload.EventTag == ActionRecoveryEndTag) OnEventActionRecoveryEnd(Payload);
}

void UActionRecoveryAbility::OnEventActionRecoveryStart(FGameplayEventData Payload)
{
	AddStateRecoveringTag();
};

void UActionRecoveryAbility::OnEventActionRecoveryEnd(FGameplayEventData Payload)
{
	RemoveStateRecoveringTags();

	//Input Buffer Play 이벤트
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEventData EventData;
		EventData.EventTag = EventPlayBufferTag;
			
		ASC->HandleGameplayEvent(EventPlayBufferTag, &EventData);
		DEBUG_LOG(TEXT("Play Buffer Event Activated"));
	}
}

void UActionRecoveryAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//RemoveStateRecoveringTags();
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}