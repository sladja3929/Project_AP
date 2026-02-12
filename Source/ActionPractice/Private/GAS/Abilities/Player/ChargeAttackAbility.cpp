#include "GAS/Abilities/Player/ChargeAttackAbility.h"
#include "Input/InputBufferComponent.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/Abilities/Player/BaseAttackAbility.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogChargeAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogChargeAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

/***
* 레거시 클래스: 사용하지 않음, AttackSequenceAbility로 대체
*/

//커브 이름 상수 정의
const FName UChargeAttackAbility::CurveName_ChargeStart = TEXT("ChargeStart");

UChargeAttackAbility::UChargeAttackAbility()
{
    //공격은 서버에서 시작 (히트 판정 권한)
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    StaminaCost = 15.0f;
}

void UChargeAttackAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);

    EventNotifyResetComboTag = UGameplayTagsSubsystem::GetEventNotifyResetComboTag();
    EventNotifyChargeStartTag = UGameplayTagsSubsystem::GetEventNotifyChargeStartTag();
    
    if (!EventNotifyResetComboTag.IsValid())
    {
        DEBUG_LOG(TEXT("EventNotifyResetComboTag is not valid"));
    }
    if (!EventNotifyChargeStartTag.IsValid())
    {
        DEBUG_LOG(TEXT("EventNotifyChargeStartTag is not valid"));
    }
}

void UChargeAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
    StartWaitInputReleaseTask(true);
}

void UChargeAttackAbility::ActivateInitSettings()
{
    Super::ActivateInitSettings();

    START_WAIT_EVENT_TASK(WaitInputByBufferEventTask, EventInputByBufferTag, OnEventInputByBuffer, nullptr, false, true);

    //무기 데이터 적용, SubAttack: 차지 몽타주, Attack: 공격 실행 몽타주
    MaxComboCount = WeaponAttackData->ComboSequence.Num();
    
    bMaxCharged = false;
    bIsChargingMontage = false;
    bNoCharge = false;
    
    DEBUG_LOG(TEXT("Charge Ability Activated"));
    bCreateTask = true;
    bIsAttackMontage = false;
}

void UChargeAttackAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    //ActionRecoveryEnd 이후 구간에서 입력이 들어오면 콤보 실행
    ACharacter* Character = GetActionPracticeCharacterFromActorInfo();
    if (!Character) return;

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return;

    //서버 사이드 확인
    bool bHasRecoveringTag = false;
    if (Character->HasAuthority())
    {
        bHasRecoveringTag = ASC->HasMatchingGameplayTag(StateRecoveringAuthTag);
    }

    //클라 사이드 확인 (리슨서버는 둘 다 확인)
    if (Character->IsLocallyControlled())
    {
        bHasRecoveringTag = ASC->HasMatchingGameplayTag(StateRecoveringLocalTag);
    }
    
    if (!bHasRecoveringTag)
    {
        bNoCharge = false;
        PlayNextCharge();
        DEBUG_LOG(TEXT("Input Pressed - After Recovery"));
    }
}

void UChargeAttackAbility::SetHitDetectionConfig()
{
    if (!bIsAttackMontage) return;
    
    Super::SetHitDetectionConfig();
}

void UChargeAttackAbility::SetStaminaCost(float InStaminaCost)
{
    if (!bIsAttackMontage) InStaminaCost = 0.0f;
    else if (bMaxCharged) InStaminaCost *= 1.4f;
    
    Super::SetStaminaCost(InStaminaCost);
}

bool UChargeAttackAbility::RotateCharacter()
{
    if (!bIsAttackMontage)
    {
        return false;
    }

    return Super::RotateCharacter();
}

UAnimMontage* UChargeAttackAbility::SetMontageToPlayTask()
{
    if (ComboCounter < 0) ComboCounter = 0;
    if (!bIsAttackMontage) return WeaponAttackData->ComboSequence[ComboCounter].SubAttackMontage.Get();

    return Super::SetMontageToPlayTask();
}

void UChargeAttackAbility::StartMontageWithEventsTask()
{
    UAnimMontage* MontageToPlay = SetMontageToPlayTask();
        
    if (!MontageToPlay)
    {
        DEBUG_LOG(TEXT("No Montage to Play"));
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
    
        SetUpPlayMontageWithEventsTask();
    }

    else //태스크 중간에 몽타주 바꾸기
    {
        //PlayMontageWithEventsTask->ChangeMontageAndPlay(MontageToPlay);
    }
}

void UChargeAttackAbility::SetUpPlayMontageWithEventsTask()
{
    if (!PlayMontageWithEventsTask)
    {
        DEBUG_LOG(TEXT("No MontageWithEvents Task"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }

    //ResetCombo 노티파이 이벤트 바인딩
    PlayMontageWithEventsTask->BindNotifyEventTag(EventNotifyResetComboTag);

    //Super 먼저 호출 (부모의 커브 폴링 설정 + 에지 바인딩)
    Super::SetUpPlayMontageWithEventsTask();

    //ChargeStart 커브를 기존 폴링 목록에 추가 (커브 에지는 가상 함수로 오버라이드됨)
    PlayMontageWithEventsTask->EnableCurvePolling(CurveName_ChargeStart);
}

void UChargeAttackAbility::PlayNextCharge()
{
    ComboCounter++;
    bMaxCharged = false;
    bIsChargingMontage = false;
    
    if (ComboCounter >= MaxComboCount)
    {
        ComboCounter = 0;
    }
    
    bCreateTask = false;
    bIsAttackMontage = false;
    StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();
}

void UChargeAttackAbility::OnTaskMontageCompleted()
{
    //차지 몽타주가 끝날 때 = 완전히 차지했을 때
    if (bIsChargingMontage)
    {
        bMaxCharged = true;
        
        DEBUG_LOG(TEXT("Montage Completed - Max Charge"));
        bCreateTask = true;
        bIsAttackMontage = true;
        StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();  
        
        bIsChargingMontage = false;
    }

    else
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UChargeAttackAbility::OnTaskNotifyEventsReceived(FGameplayEventData Payload)
{
    Super::OnTaskNotifyEventsReceived(Payload);

    if (Payload.EventTag == EventNotifyResetComboTag) OnNotifyResetCombo(Payload);
}

void UChargeAttackAbility::OnNotifyResetCombo(FGameplayEventData Payload)
{
    DEBUG_LOG(TEXT("Reset Combo"));
    ComboCounter = -1; //어빌리티가 살아있는 동안 입력이 들어오면 PlayNext로 0이 되고, 어빌리티가 죽으면 초기화
}

void UChargeAttackAbility::OnCurveRisingEdgeReceived(FName CurveName)
{
    Super::OnCurveRisingEdgeReceived(CurveName);

    if (CurveName == CurveName_ChargeStart)
    {
        DEBUG_LOG(TEXT("Charge Start (Curve Rising Edge)"));
        bIsChargingMontage = true;
      
        if (bNoCharge) //이미 뗴져 있다면 바로 공격
        {
            bCreateTask = false;
            bIsAttackMontage = true;
            StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();

            bIsChargingMontage = false;
            bNoCharge = false;
        }
    }
}

void UChargeAttackAbility::OnEventInputByBuffer(FGameplayEventData Payload)
{
    if (!Payload.InstigatorTags.HasTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.ChargeAttack"))))) return;
    
    bNoCharge = Payload.EventMagnitude != 0.0f;
    PlayNextCharge();
    DEBUG_LOG(TEXT("Input By Buffer - Play Next Charge %s"), bNoCharge ? TEXT("No Charge") : TEXT("Charge"));
}

void UChargeAttackAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
    if (bMaxCharged) AttackData.FinalDamage *= 1.5f;
    
    Super::OnHitDetected(HitActor, HitResult, AttackData);
}

void UChargeAttackAbility::OnWaitInputRelease(float TimeHeld)
{
    //차지를 멈췄을 때
    if (bIsChargingMontage) //차지중이라면
    {
        bCreateTask = false;
        bIsAttackMontage = true;
        StartWaitDelayTask_WaitRotateCharacterAndPlayMontageTask();

        bIsChargingMontage = false;
    }

    else //차지중이 아니라면 (선딜 전에 뗌)
    {
        DEBUG_LOG(TEXT("Input Released - No Charge true"));    
        bNoCharge = true;
    }
}

void UChargeAttackAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    DEBUG_LOG(TEXT("AttackAbility Cancelled"));    
    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UChargeAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ComboCounter = 0;
    bMaxCharged = false;
    bIsChargingMontage = false;
    bNoCharge = false;
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

