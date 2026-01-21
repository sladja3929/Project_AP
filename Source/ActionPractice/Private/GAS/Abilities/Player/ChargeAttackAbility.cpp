#include "GAS/Abilities/Player/ChargeAttackAbility.h"
#include "Input/InputBufferComponent.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GAS/Abilities/Player/BaseAttackAbility.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogChargeAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogChargeAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

//커브 이름 상수 정의
const FName UChargeAttackAbility::CurveName_ChargeStart = TEXT("ChargeStart");

UChargeAttackAbility::UChargeAttackAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    StaminaCost = 15.0f;
}

void UChargeAttackAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

    EventNotifyResetComboTag = UGameplayTagsSubsystem::GetEventNotifyResetComboTag();

    if (!EventNotifyResetComboTag.IsValid())
    {
        DEBUG_LOG(TEXT("EventNotifyResetComboTag is not valid"));
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

    ReadyInputByBufferTask();

    //무기 데이터 적용, SubAttack: 차지 몽타주, Attack: 공격 실행 몽타주
    MaxComboCount = WeaponAttackData->ComboSequence.Num();

    bMaxCharged = false;
    bIsAttackMontage = false;

    //Phase/Release 초기화
    ChargePhase = EAPChargePhase::Charging_PreStart;
    bReleaseRequested = false;
    if (UInputBufferComponent* BufferComp = GetInputBufferComponentFromActorInfo())
    {
        bReleaseRequested = BufferComp->bBufferActionReleased;
    }

    DEBUG_LOG(TEXT("Charge Ability Activated"));
    bCreateTask = true;
}

void UChargeAttackAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    //ActionRecoveryEnd 이후 구간에서 입력이 들어오면 콤보 실행
    if (!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(StateRecoveringTag))
    {
        //TransitionToAttack guard가 다시 열리도록 새 사이클 초기화
        PlayNextCharge(false);
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

void UChargeAttackAbility::ExecuteMontageTask()
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
    
        BindEventsAndReadyMontageTask();
    }

    else //태스크 중간에 몽타주 바꾸기
    {
        PlayMontageWithEventsTask->ChangeMontageAndPlay(MontageToPlay);
    }
}

void UChargeAttackAbility::BindEventsAndReadyMontageTask()
{
    if (!PlayMontageWithEventsTask)
    {
        DEBUG_LOG(TEXT("No MontageWithEvents Task"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }

    //ResetCombo 노티파이 이벤트 바인딩
    PlayMontageWithEventsTask->BindNotifyEventCallbackWithTag(EventNotifyResetComboTag);

    //Super 먼저 호출 (부모의 커브 폴링 설정 + 에지 바인딩)
    Super::BindEventsAndReadyMontageTask();

    //ChargeStart 커브를 기존 폴링 목록에 추가 (커브 에지는 가상 함수로 오버라이드됨)
    PlayMontageWithEventsTask->AddCurveToPolling(CurveName_ChargeStart);
}

void UChargeAttackAbility::TransitionToAttack(bool bInMaxCharged, bool bInCreateNewTask)
{
    //같은 사이클에서 중복 전환 방지
    if (ChargePhase == EAPChargePhase::Attack)
    {
        return;
    }

    ChargePhase = EAPChargePhase::Attack;

    //Attack으로 넘어가는 순간 WaitInputReleaseTask 정리
    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->EndTask();
        WaitInputReleaseTask = nullptr;
    }

    bMaxCharged = bInMaxCharged;
    bIsAttackMontage = true;

    //진행 중 전환(Release)은 ChangeMontage, Charge 완료 전환은 새 Task
    bCreateTask = bInCreateNewTask;

    PlayAction();
}

void UChargeAttackAbility::PlayNextCharge(bool bInReleaseRequested)
{
    ComboCounter++;

    if (ComboCounter >= MaxComboCount)
    {
        ComboCounter = 0;
    }

    //다음 콤보 시작 시 TransitionToAttack 실행 가능 상태 초기화
    ChargePhase = EAPChargePhase::Charging_PreStart;
    bReleaseRequested = bInReleaseRequested;

    bMaxCharged = false;
    bIsAttackMontage = false;
    bCreateTask = (PlayMontageWithEventsTask == nullptr);

    //릴리즈 감지도 매 콤보별 갱신
    StartWaitInputReleaseTask(true);

    PlayAction();
}

void UChargeAttackAbility::OnTaskMontageCompleted()
{
    //Charge 몽타주가 끝난 상황에서 아직 Attack으로 안 넘어갔다면 풀차지
    if (ChargePhase == EAPChargePhase::Charging_Active && !bReleaseRequested)
    {
        DEBUG_LOG(TEXT("Montage Completed - Max Charge"));
        TransitionToAttack(true, true); // Charge가 끝났으니 새 Task 생성으로 Attack 재생
        return;
    }

    //Attack 몽타주 완료는 능력 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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

        ChargePhase = EAPChargePhase::Charging_Active;

        //ChargeStart 전에 Release -> ChargeStart 들어오면 바로 Attack
        if (bReleaseRequested)
        {
            TransitionToAttack(false, false); //진행 중이므로 ChangeMontage로 전환
        }
    }
}

void UChargeAttackAbility::OnEventInputByBuffer(FGameplayEventData Payload)
{
    if (Payload.OptionalObject && Payload.OptionalObject != this) return;

    const bool bBufferedRelease = (Payload.EventMagnitude != 0.0f);
    
    //TransitionToAttack guard가 다시 열리도록 새 사이클 초기화
    PlayNextCharge(bBufferedRelease);

    DEBUG_LOG(TEXT("Input By Buffer - Play Next Charge"));
}

void UChargeAttackAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
    if (bMaxCharged) AttackData.FinalDamage *= 1.5f;
    
    Super::OnHitDetected(HitActor, HitResult, AttackData);
}

void UChargeAttackAbility::HandleWaitInputReleased(float TimeHeld)
{
    bReleaseRequested = true;

    //ChargeStart 이후면 즉시 Attack으로 전환
    if (ChargePhase == EAPChargePhase::Charging_Active)
    {
        TransitionToAttack(false, false); // 진행 중이므로 ChangeMontage
        return;
    }

    DEBUG_LOG(TEXT("HandleWaitInputReleased - Waiting ChargeStart"));
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
    bIsAttackMontage = false;

    ChargePhase = EAPChargePhase::Charging_PreStart;
    bReleaseRequested = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}