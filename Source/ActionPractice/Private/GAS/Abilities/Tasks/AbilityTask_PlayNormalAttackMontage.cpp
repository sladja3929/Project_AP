#include "GAS/Abilities/Tasks/AbilityTask_PlayNormalAttackMontage.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/GameplayTagsSubsystem.h"

// 디버그 로그 활성화/비활성화 (0: 비활성화, 1: 활성화)
#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAbilityTask_PlayNormalAttackMontage, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAbilityTask_PlayNormalAttackMontage, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UAbilityTask_PlayNormalAttackMontage::UAbilityTask_PlayNormalAttackMontage(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Rate = 1.0f;
    bStopMontageWhenAbilityCancelled = false;
    
    ComboCounter = 0;
    MaxComboCount = 3;
    bCanComboSave = false;
    bComboInputSaved = false;
    bIsInCancellableRecovery = false;
    bIsTransitioningToNextCombo = false;
    
    CurrentMontage = nullptr;
}

UAbilityTask_PlayNormalAttackMontage* UAbilityTask_PlayNormalAttackMontage::CreatePlayNormalAttackMontageProxy(
    UGameplayAbility* OwningAbility,
    FName TaskInstanceName,
    const TArray<TSoftObjectPtr<UAnimMontage>>& MontagesToPlay,
    float Rate,
    FName StartSection,
    float AnimRootMotionTranslationScale)
{
    UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

    UAbilityTask_PlayNormalAttackMontage* MyTask = NewAbilityTask<UAbilityTask_PlayNormalAttackMontage>(OwningAbility, TaskInstanceName);
    MyTask->MontagesToPlay = MontagesToPlay;
    MyTask->Rate = Rate;
    MyTask->StartSectionName = StartSection;
    MyTask->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;

    return MyTask;
}

void UAbilityTask_PlayNormalAttackMontage::Activate()
{
    if (Ability == nullptr)
    {
        return;
    }
    
    DEBUG_LOG(TEXT("Task Activate"));
    
    bool bPlayedMontage = false;
    
    if (AbilitySystemComponent.IsValid())
    {
        const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
        UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

        if (AnimInstance != nullptr)
        {
            // 이벤트 콜백 등록
            RegisterGameplayEventCallbacks();
            
            // 첫 공격 실행
            PlayAttackMontage();
            bPlayedMontage = true;
        }
    }

    if (!bPlayedMontage)
    {
        if (ShouldBroadcastAbilityTaskDelegates())
        {
            OnCancelled.Broadcast();
        }
        EndTask();
    }

    SetWaitingOnAvatar();
}

#pragma region "Attack Functions"
void UAbilityTask_PlayNormalAttackMontage::PlayAttackMontage()
{
    const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
    UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
    
    if (!AnimInstance)
    {
        EndTask();
        return;
    }
    
    // 몽타주 배열 검증
    if (MontagesToPlay.IsEmpty() || ComboCounter >= MontagesToPlay.Num())
    {
        DEBUG_LOG(TEXT("Invalid montage array or combo counter"));
        EndTask();
        return;
    }
    
    // 이전 몽타주의 델리게이트 정리 (새 몽타주 로드 전에)
    UAnimMontage* PreviousMontage = CurrentMontage;
    if (PreviousMontage)
    {
        if (BlendingOutDelegate.IsBound())
        {
            FOnMontageBlendingOutStarted EmptyBlendDelegate;
            AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendDelegate, PreviousMontage);
            BlendingOutDelegate.Unbind();
        }
        if (MontageEndedDelegate.IsBound())
        {
            FOnMontageEnded EmptyEndDelegate;
            AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, PreviousMontage);
            MontageEndedDelegate.Unbind();
        }
        DEBUG_LOG(TEXT("Cleared delegates for previous montage: %s"), *PreviousMontage->GetName());
    }
    
    // 현재 콤보에 해당하는 몽타주 로드
    CurrentMontage = MontagesToPlay[ComboCounter].LoadSynchronous();
    if (!CurrentMontage)
    {
        DEBUG_LOG(TEXT("Failed to load montage at index %d"), ComboCounter);
        EndTask();
        return;
    }

    bComboInputSaved = false;
    bCanComboSave = false;
    bIsInCancellableRecovery = false;
    // bIsTransitioningToNextCombo는 OnMontageEnded에서만 리셋 (타이밍 문제 방지)

    if (AbilitySystemComponent.IsValid())
    {
        AbilitySystemComponent->AddLooseGameplayTag(UGameplayTagsSubsystem::GetStateRecoveringTag());
    }
    
    float PlayLength = AnimInstance->Montage_Play(CurrentMontage, Rate);
    DEBUG_LOG(TEXT("Montage Play Result: %f, Montage Name: %s"), PlayLength, CurrentMontage ? *CurrentMontage->GetName() : TEXT("NULL"));

    // 블렌드 아웃 델리게이트 바인딩
    BlendingOutDelegate = FOnMontageBlendingOutStarted::CreateUObject(this, &UAbilityTask_PlayNormalAttackMontage::OnMontageBlendingOut);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, CurrentMontage);
    DEBUG_LOG(TEXT("BlendingOutDelegate Bound Successfully"));

    // 몽타주 종료 델리게이트 바인딩
    MontageEndedDelegate = FOnMontageEnded::CreateUObject(this, &UAbilityTask_PlayNormalAttackMontage::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, CurrentMontage);
    DEBUG_LOG(TEXT("MontageEndedDelegate Bound Successfully"));

    ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
    if (Character && (Character->GetLocalRole() == ROLE_Authority ||
        (Character->GetLocalRole() == ROLE_AutonomousProxy && 
         Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
    {
        Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
    }

    // 콤보 실행 알림
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnComboPerformed.Broadcast();
    }
    
    DEBUG_LOG(TEXT("Attack Monatage First Played"));
}

void UAbilityTask_PlayNormalAttackMontage::PlayNextAttackCombo()
{
    ComboCounter++;
    
    if (ComboCounter >= MaxComboCount)
    {
        DEBUG_LOG(TEXT("Max combo reached - ending task"));
        EndTask();
        return;
    }
    
    // 몽타주 전환 플래그 설정 (델리게이트에서 OnInterrupted 무시하기 위함)
    bIsTransitioningToNextCombo = true;
    
    DEBUG_LOG(TEXT("Starting combo %d transition"), ComboCounter + 1);
    
    // PlayAttackMontage를 재사용하여 다음 몽타주 재생
    PlayAttackMontage();
}

void UAbilityTask_PlayNormalAttackMontage::CheckComboInputPreseed() //어빌리티 실행 중 입력이 들어올 때
{
    // 2. enablecomboInput 구간에서 입력이 들어오면 저장
    if (bCanComboSave)
    {
        bComboInputSaved = true;
        bCanComboSave = false;
        
        DEBUG_LOG(TEXT("Combo Saved"));
    }
    // 3-2. ActionRecoveryEnd 이후 구간에서 입력이 들어오면 콤보 실행
    else if (bIsInCancellableRecovery)
    {
        PlayNextAttackCombo();

        DEBUG_LOG(TEXT("Combo Played After Recovery"));
    }
}

#pragma endregion

#pragma region "AnimNotify Event Functions"
void UAbilityTask_PlayNormalAttackMontage::HandleEnableBufferInputEvent(const FGameplayEventData& Payload)
{
    bCanComboSave = true;
}

void UAbilityTask_PlayNormalAttackMontage::HandleActionRecoveryEndEvent(const FGameplayEventData& Payload)
{
    bCanComboSave = false;

    // 3-1. 2~3 사이 저장한 행동이 있을 경우
    if (bComboInputSaved)
    {
        PlayNextAttackCombo();

        DEBUG_LOG(TEXT("Combo Played With Saved"));
    }
    // 3-2. 저장한 행동이 없을 경우
    else
    {
        if (AbilitySystemComponent.IsValid())
        {
            // 모든 StateRecovering 태그 제거 (스택된 태그 모두 제거)
            while (AbilitySystemComponent->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateRecoveringTag()))
            {
                AbilitySystemComponent->RemoveLooseGameplayTag(UGameplayTagsSubsystem::GetStateRecoveringTag());
            }
            DEBUG_LOG(TEXT("Can ABP Interrupt Attack Montage"));
        }
 
        bIsInCancellableRecovery = true;
    }
}

// 4. ResetCombo 이벤트 수신
void UAbilityTask_PlayNormalAttackMontage::HandleResetComboEvent(const FGameplayEventData& Payload)
{    
    // EndTask()를 바로 호출하지 않고 몽타주 종료를 기다림
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnCompleted.Broadcast();
    }
}

void UAbilityTask_PlayNormalAttackMontage::RegisterGameplayEventCallbacks()
{
    if (AbilitySystemComponent.IsValid() && Ability)
    {
        // EnableComboInput 이벤트 - Lambda 사용
        EnableBufferInputHandle = AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyEnableBufferInputTag())
            .AddLambda([this](const FGameplayEventData* EventData)
            {
                if (IsValid(this) && EventData)
                {
                    HandleEnableBufferInputEvent(*EventData);
                }
            });

        // ActionRecoveryEnd 이벤트 - Lambda 사용
        ActionRecoveryEndHandle = AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyActionRecoveryEndTag())
            .AddLambda([this](const FGameplayEventData* EventData)
            {
                if (IsValid(this) && EventData)
                {
                    HandleActionRecoveryEndEvent(*EventData);
                }
            });

        // ResetCombo 이벤트 - Lambda 사용
        ResetComboHandle = AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyResetComboTag())
            .AddLambda([this](const FGameplayEventData* EventData)
            {
                if (IsValid(this) && EventData)
                {
                    HandleResetComboEvent(*EventData);
                }
            });
    }
}

void UAbilityTask_PlayNormalAttackMontage::UnregisterGameplayEventCallbacks()
{
    if (AbilitySystemComponent.IsValid())
    {
        if (EnableBufferInputHandle.IsValid())
        {
            AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyEnableBufferInputTag())
                .Remove(EnableBufferInputHandle);
        }

        if (ActionRecoveryEndHandle.IsValid())
        {
            AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyActionRecoveryEndTag())
                .Remove(ActionRecoveryEndHandle);
        }

        if (ResetComboHandle.IsValid())
        {
            AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyResetComboTag())
                .Remove(ResetComboHandle);
        }
    }
}
#pragma endregion

#pragma region "Montage Functions"
void UAbilityTask_PlayNormalAttackMontage::StopPlayingMontage()
{
    const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
    if (!ActorInfo)
    {
        return;
    }

    UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
    if (AnimInstance && CurrentMontage)
    {
        float BlendOutTime = CurrentMontage->BlendOut.GetBlendTime();
        AnimInstance->Montage_Stop(BlendOutTime, CurrentMontage);
    }
}

void UAbilityTask_PlayNormalAttackMontage::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    DEBUG_LOG(TEXT("OnMontageBlendingOut Called - Montage: %s, Interrupted: %s"), 
           Montage ? *Montage->GetName() : TEXT("None"), 
           bInterrupted ? TEXT("True") : TEXT("False"));
           
    if (Ability && Ability->GetCurrentMontage() == CurrentMontage)
    {
        if (Montage == CurrentMontage)
        {
            AbilitySystemComponent->ClearAnimatingAbility(Ability);

            ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
            if (Character && (Character->GetLocalRole() == ROLE_Authority ||
                (Character->GetLocalRole() == ROLE_AutonomousProxy && 
                 Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
            {
                Character->SetAnimRootMotionTranslationScale(1.0f);
            }
        }
    }

    if (bInterrupted)
    {
        // 콤보 전환이 아닌 경우에만 OnInterrupted 브로드캐스트
        if (!bIsTransitioningToNextCombo && ShouldBroadcastAbilityTaskDelegates())
        {
            OnInterrupted.Broadcast();
        }
    }
    else
    {
        if (ShouldBroadcastAbilityTaskDelegates())
        {
            OnBlendOut.Broadcast();
        }
    }
}

void UAbilityTask_PlayNormalAttackMontage::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    DEBUG_LOG(TEXT("OnMontageEnded Called - Montage: %s, Interrupted: %s"), 
           Montage ? *Montage->GetName() : TEXT("None"), 
           bInterrupted ? TEXT("True") : TEXT("False"));
    
    // 콤보 전환으로 인한 종료인지 확인
    if (bIsTransitioningToNextCombo && bInterrupted)
    {
        DEBUG_LOG(TEXT("Combo transition detected - not ending task"));
        bIsTransitioningToNextCombo = false; // 플래그 리셋
        return; // 콤보 진행 중이므로 태스크 종료하지 않음
    }
           
    if (!bInterrupted)
    {
        if (ShouldBroadcastAbilityTaskDelegates())
        {
            OnCompleted.Broadcast();
        }
    }
    else
    {
        // 콤보 전환이 아닌 경우에만 OnInterrupted 브로드캐스트
        if (!bIsTransitioningToNextCombo && ShouldBroadcastAbilityTaskDelegates())
        {
            OnInterrupted.Broadcast();
        }
    }

    EndTask();
}

void UAbilityTask_PlayNormalAttackMontage::ExternalCancel()
{
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnCancelled.Broadcast();
    }

    Super::ExternalCancel();
}
#pragma endregion

void UAbilityTask_PlayNormalAttackMontage::OnDestroy(bool AbilityEnded)
{
    DEBUG_LOG(TEXT("Normal Attack Task Destroyed"));

    // 몽타주 정지
    if (bStopMontageWhenAbilityCancelled)
    {
        StopPlayingMontage();
    }
    
    // 이벤트 콜백 해제
    UnregisterGameplayEventCallbacks();
    
    // 몽타주 델리게이트 정리
    const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
    if (ActorInfo)
    {
        UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
        if (AnimInstance && CurrentMontage)
        {
            if (BlendingOutDelegate.IsBound())
            {
                FOnMontageBlendingOutStarted EmptyBlendDelegate;
                AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendDelegate, CurrentMontage);
                BlendingOutDelegate.Unbind();
            }
            if (MontageEndedDelegate.IsBound())
            {
                FOnMontageEnded EmptyEndDelegate;
                AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, CurrentMontage);
                MontageEndedDelegate.Unbind();
            }
        }
    }
    
    // 상태 정리 - 모든 StateRecovering 태그 제거
    if (AbilitySystemComponent.IsValid())
    {
        while (AbilitySystemComponent->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateRecoveringTag()))
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(UGameplayTagsSubsystem::GetStateRecoveringTag());
        }
        DEBUG_LOG(TEXT("All StateRecovering tags removed"));
    }
    
    bCanComboSave = false;
    bComboInputSaved = false;
    bIsInCancellableRecovery = false;
    bIsTransitioningToNextCombo = false;
    bStopMontageWhenAbilityCancelled = false;
    
    // 포인터 정리
    CurrentMontage = nullptr;
    MontagesToPlay.Empty();

    Super::OnDestroy(AbilityEnded);
}