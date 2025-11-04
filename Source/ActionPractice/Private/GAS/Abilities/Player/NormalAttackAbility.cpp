#include "GAS/Abilities/Player/NormalAttackAbility.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogNormalAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogNormalAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UNormalAttackAbility::UNormalAttackAbility()
{
    StaminaCost = 15.0f;
}

void UNormalAttackAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EventNotifyResetComboTag = UGameplayTagsSubsystem::GetEventNotifyResetComboTag();

	if (!EventNotifyResetComboTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyResetComboTag is not valid"));
	}
}

void UNormalAttackAbility::ActivateInitSettings()
{
    Super::ActivateInitSettings();

    ReadyInputByBufferTask();

    //무기 데이터 적용
    MaxComboCount = WeaponAttackData->ComboSequence.Num();
    
    bCreateTask = true;
}

void UNormalAttackAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    //ActionRecoveryEnd 이후 구간에서 입력이 들어오면 콤보 실행
    if (!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(StateRecoveringTag))
    {
        PlayNextAttack();
        DEBUG_LOG(TEXT("Input Pressed - After Recovery"));
    }
}


void UNormalAttackAbility::PlayNextAttack()
{
    ++ComboCounter;
    
    if (ComboCounter >= MaxComboCount)
    {
        ComboCounter = 0;
    }
    
    DEBUG_LOG(TEXT("NextAttack - ComboCounter: %d"),ComboCounter);

    bCreateTask = false;
    PlayAction();
}

/* 공격 수행 메커니즘
 * 1. 몽타주 실행 (State.IsRecovering 태그 추가)
 * 2. EnableComboInput = 입력 저장 가능 구간, 다음 공격과 구르기 저장 가능 (구르기를 저장해도 다음 공격 우선 저장)
 * 3. ActionRecoveryEnd = 공격 선딜이 끝나는 지점
 * 3-1. 2~3 사이 저장한 행동이 있을 경우 CheckComboInput으로 행동 수행
 * 3-2. 2~3 사이 저장한 행동이 없을 경우 입력이 들어오면 다음 공격 가능, 이동/점프/구르기로 캔슬 가능 (State.IsRecovering 태그 제거)
 * 4. ResetCombo = 공격 콤보가 초기화되어 다음 콤보로 연계되지 않음
 * 5. 몽타주 종료 (ResetCombo와 같지 않음)
 */

void UNormalAttackAbility::ExecuteMontageTask()
{
    UAnimMontage* MontageToPlay = SetMontageToPlayTask();
    if (!MontageToPlay)
    {
        DEBUG_LOG(TEXT("No Montage to Play %d"), ComboCounter);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }
    
    if (bCreateTask) //커스텀 태스크 생성
    {        
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

    else //태스크 중간에 몽타주 바꾸기
    {
        PlayMontageWithEventsTask->ChangeMontageAndPlay(MontageToPlay);
    }
}

void UNormalAttackAbility::BindEventsAndReadyMontageTask()
{
    if (!PlayMontageWithEventsTask)
    {
        DEBUG_LOG(TEXT("No MontageWithEvents Task"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }

    //ResetCombo 노티파이 이벤트 바인딩
    PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyResetComboTag);
    
    Super::BindEventsAndReadyMontageTask();
}

void UNormalAttackAbility::OnTaskNotifyEventsReceived(FGameplayEventData Payload)
{
    Super::OnTaskNotifyEventsReceived(Payload);

    if (Payload.EventTag == EventNotifyResetComboTag) OnNotifyResetCombo(Payload);
}

void UNormalAttackAbility::OnEventInputByBuffer(FGameplayEventData Payload)
{
    if (Payload.OptionalObject && Payload.OptionalObject != this) return;
    
    PlayNextAttack();
    DEBUG_LOG(TEXT("Attack Recovery End - Play Next Attack"));
}

void UNormalAttackAbility::OnNotifyResetCombo(FGameplayEventData Payload)
{
    DEBUG_LOG(TEXT("Reset Combo"));
    ComboCounter = -1; //어빌리티가 살아있는 동안 입력이 들어오면 PlayNext로 0이 되고, 어빌리티가 죽으면 초기화
}

void UNormalAttackAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    DEBUG_LOG(TEXT("AttackAbility Cancelled"));    
    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UNormalAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ComboCounter = 0;
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}