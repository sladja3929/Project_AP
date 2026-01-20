#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAbilityTask_PlayMontageWithEvents, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAbilityTask_PlayMontageWithEvents, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UAbilityTask_PlayMontageWithEvents::UAbilityTask_PlayMontageWithEvents(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    //틱 태스크 - 커브 폴링 사용 시 활성화됨
    bTickingTask = false;
}

UAbilityTask_PlayMontageWithEvents* UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
    UGameplayAbility* OwningAbility,
    FName TaskInstanceName,
    UAnimMontage* MontageToPlay,
    float Rate,
    FName StartSection,
    float AnimRootMotionTranslationScale)
{
    UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

    UAbilityTask_PlayMontageWithEvents* MyTask = NewAbilityTask<UAbilityTask_PlayMontageWithEvents>(OwningAbility, TaskInstanceName);
    MyTask->MontageToPlay = MontageToPlay;
    MyTask->Rate = Rate;
    MyTask->StartSectionName = StartSection;
    MyTask->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;

    return MyTask;
}

void UAbilityTask_PlayMontageWithEvents::Activate()
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
            //추후 태스크 생성에서 태그 컨테이너를 넘기면 활성화
            //BindAllEventCallbacks();            
            PlayMontage();
            bPlayedMontage = true;
        }
    }

    if (!bPlayedMontage)
    {
        EndTask();
    }

    SetWaitingOnAvatar();
}

void UAbilityTask_PlayMontageWithEvents::TickTask(float DeltaTime)
{
    Super::TickTask(DeltaTime);

    if (!bUseCurvePolling || !Ability || !AbilitySystemComponent.IsValid())
    {
        return;
    }

    const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
    UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;

    if (AnimInstance)
    {
        //커브 폴링 실행
        CurvePoller.Poll(AnimInstance);

        //에지 브로드캐스트
        if (ShouldBroadcastAbilityTaskDelegates())
        {
            for (const FName& CurveName : CurvePoller.RisingEdgeCurves)
            {
                DEBUG_LOG(TEXT("Curve Rising Edge: %s"), *CurveName.ToString());
                OnCurveRisingEdge.Broadcast(CurveName);
            }

            for (const FName& CurveName : CurvePoller.FallingEdgeCurves)
            {
                DEBUG_LOG(TEXT("Curve Falling Edge: %s"), *CurveName.ToString());
                OnCurveFallingEdge.Broadcast(CurveName);
            }
        }
    }
}

#pragma region "Curve Polling Functions"
void UAbilityTask_PlayMontageWithEvents::EnableCurvePolling(const TArray<FName>& CurveNames)
{
    CurvePoller.Initialize(CurveNames);
    bUseCurvePolling = true;
    bTickingTask = true;
    DEBUG_LOG(TEXT("Curve Polling Enabled with %d curves"), CurveNames.Num());
}

void UAbilityTask_PlayMontageWithEvents::DisableCurvePolling()
{
    bUseCurvePolling = false;
    bTickingTask = false;
    CurvePoller.Reset();
    DEBUG_LOG(TEXT("Curve Polling Disabled"));
}

void UAbilityTask_PlayMontageWithEvents::ResetCurvePoller()
{
    CurvePoller.Reset();
    DEBUG_LOG(TEXT("Curve Poller Reset"));
}
#pragma endregion

#pragma region "Montage Play Functions"
void UAbilityTask_PlayMontageWithEvents::PlayMontage()
{
    const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
    UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

    if (!AnimInstance)
    {
        EndTask();
        return;
    }

    if (!MontageToPlay)
    {
        DEBUG_LOG(TEXT("Invalid montage"));
        EndTask();
        return;
    }

    //ASC의 PlayMontage를 사용하여 네트워크 복제 지원
    float PlayLength = AbilitySystemComponent->PlayMontage(
        Ability,
        Ability->GetCurrentActivationInfo(),
        MontageToPlay,
        Rate,
        StartSectionName
    );
    DEBUG_LOG(TEXT("Montage Play Result: %f, Montage Name: %s"), PlayLength, MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL"));

    BindMontageCallbacks();

    ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
    if (Character && (Character->GetLocalRole() == ROLE_Authority ||
        (Character->GetLocalRole() == ROLE_AutonomousProxy &&
         Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
    {
        Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
    }
}

void UAbilityTask_PlayMontageWithEvents::ChangeMontageAndPlay(UAnimMontage* NewMontage)
{
    UAnimInstance* AnimInstance = Ability->GetCurrentActorInfo()->GetAnimInstance();
    if (!NewMontage || !IsActive() || !AnimInstance)
    {
        DEBUG_LOG(TEXT("No Montage or AnimInstance or Task"));
        //return;
    }

    bStopBroadCastMontageEvents = true;
    UnbindMontageCallbacks();
    StopPlayingMontage();

    MontageToPlay = NewMontage;

    //ASC의 PlayMontage를 사용하여 네트워크 복제 지원
    float PlayLength = AbilitySystemComponent->PlayMontage(
        Ability,
        Ability->GetCurrentActivationInfo(),
        MontageToPlay,
        Rate,
        StartSectionName
    );
    DEBUG_LOG(TEXT("Montage Play Result: %f, Montage Name: %s"), PlayLength, MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL"));

    if (PlayLength > 0.0f)
    {
        //새 콜백 바인딩
        BindMontageCallbacks();
        bStopBroadCastMontageEvents = false;
    }
}

void UAbilityTask_PlayMontageWithEvents::StopPlayingMontage()
{
    if (!AbilitySystemComponent.IsValid())
    {
        return;
    }

    // 내가 재생했던 몽타주가 아니면, 다른 어빌리티(예: Roll)가 막 재생한 몽타주를 끊어버릴 수 있음.
    if (!MontageToPlay)
    {
        return;
    }

    UAnimMontage* CurrentMontage = AbilitySystemComponent->GetCurrentMontage();
    if (CurrentMontage != MontageToPlay)
    {
        DEBUG_LOG(TEXT("StopPlayingMontage skipped. Current=%s, Mine=%s"),
            *GetNameSafe(CurrentMontage),
            *GetNameSafe(MontageToPlay));
        return;
    }

    AbilitySystemComponent->CurrentMontageStop();

    DEBUG_LOG(TEXT("StopPlayingMontage executed. Montage=%s"), *GetNameSafe(MontageToPlay));
}
#pragma endregion

#pragma region "Event Calling Functions"
void UAbilityTask_PlayMontageWithEvents::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    DEBUG_LOG(TEXT("OnMontageBlendingOut Called - Montage: %s, Interrupted: %s"), 
           Montage ? *Montage->GetName() : TEXT("None"), 
           bInterrupted ? TEXT("True") : TEXT("False"));
           
    if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
    {
        if (Montage == MontageToPlay)
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

    if (ShouldBroadcastAbilityTaskDelegates() && !bStopBroadCastMontageEvents)
    {
        OnMontageBlendOut.Broadcast();
    }
}

void UAbilityTask_PlayMontageWithEvents::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    DEBUG_LOG(TEXT("OnMontageEnded Called - Montage: %s, Interrupted: %s"), 
           Montage ? *Montage->GetName() : TEXT("None"), 
           bInterrupted ? TEXT("True") : TEXT("False"));
           
    if (!bInterrupted)
    {
        if (ShouldBroadcastAbilityTaskDelegates() && !bStopBroadCastMontageEvents)
        {
            OnMontageCompleted.Broadcast();
        }
    }
    else
    {
        if (ShouldBroadcastAbilityTaskDelegates() && !bStopBroadCastMontageEvents)
        {
            OnMontageInterrupted.Broadcast();
        }
    }

    EndTask();
}

void UAbilityTask_PlayMontageWithEvents::HandleNotifyEvents(const FGameplayEventData& Payload)
{
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        //Payload를 그대로 넘겨서 어빌리티에서 이벤트 종류를 판별하도록 함
        OnNotifyEventsReceived.Broadcast(Payload);
    }
}

#pragma endregion

#pragma region "Delegate Binding Functions"
void UAbilityTask_PlayMontageWithEvents::BindNotifyEventCallbackWithTag(FGameplayTag EventTag)
{
    EventTagsToReceive.AddTag(EventTag);

    if (!AbilitySystemComponent.IsValid() || !Ability)
    {
        return;
    }

    FDelegateHandle Handle = AbilitySystemComponent->GenericGameplayEventCallbacks
       .FindOrAdd(EventTag)
       .AddLambda([this](const FGameplayEventData* EventData)
       {
           if (IsValid(this) && EventData)
           {
               HandleNotifyEvents(*EventData);
           }
       });

    //Map에 핸들 저장
    EventHandles.Add(EventTag, Handle);
    DEBUG_LOG(TEXT("Event Callback Bound - Tag: %s"), *EventTag.ToString());
}

void UAbilityTask_PlayMontageWithEvents::UnbindNotifyEventCallbackWithTag(FGameplayTag EventTag)
{
    if (!AbilitySystemComponent.IsValid())
    {
        return;
    }

    //TMap에 태그에 따른 핸들이 존재하고, 해당 핸들이 유효할 경우 (핸들이 존재하면 무조건 삭제)
    FDelegateHandle Handle;    
    if (EventHandles.RemoveAndCopyValue(EventTag, Handle) && Handle.IsValid())
    {
        AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(EventTag).Remove(Handle);
        DEBUG_LOG(TEXT("Event Callback Unbound - Tag: %s"), *EventTag.ToString());
    }

    EventTagsToReceive.RemoveTag(EventTag);
}

void UAbilityTask_PlayMontageWithEvents::BindAllEventCallbacks()
{
    if (!AbilitySystemComponent.IsValid() || !Ability)
    {
        return;
    }

    //현재 저장된 태그 컨테이너를 순회하며 바인딩
    for (const FGameplayTag& Tag: EventTagsToReceive)
    {
        FDelegateHandle Handle = AbilitySystemComponent->GenericGameplayEventCallbacks
        .FindOrAdd(Tag)
        .AddLambda([this](const FGameplayEventData* EventData)
        {
            if (IsValid(this) && EventData)
            {
                HandleNotifyEvents(*EventData);
            }
        });

        //Map에 핸들 저장
        EventHandles.Add(Tag, Handle);
        DEBUG_LOG(TEXT("All Event Callback Bound - Tag: %s"), *Tag.ToString());
    }
}

void UAbilityTask_PlayMontageWithEvents::UnbindAllEventCallbacks()
{
    if (!AbilitySystemComponent.IsValid())
    {
        return;
    }

    //저장된 이벤트 핸들 순회
    for (const auto& Pair: EventHandles)
    {
        const FGameplayTag& Tag = Pair.Key;
        const FDelegateHandle& Handle = Pair.Value;

        if (Handle.IsValid())
        {
            AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(Tag).Remove(Handle);
            DEBUG_LOG(TEXT("All Event Callback Unbound - Tag: %s"), *Tag.ToString());
        }
    }

    EventHandles.Empty();
}

void UAbilityTask_PlayMontageWithEvents::BindMontageCallbacks()
{
    const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
    UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
    
    if (!AnimInstance || !MontageToPlay)
    {
        return;
    }

    OnBlendingOutInternal = FOnMontageBlendingOutStarted::CreateUObject(this, &UAbilityTask_PlayMontageWithEvents::OnMontageBlendingOut);
    AnimInstance->Montage_SetBlendingOutDelegate(OnBlendingOutInternal, MontageToPlay);
    DEBUG_LOG(TEXT("BlendingOutDelegate Bound Successfully, Montage Name: %s") ,MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL"));

    OnMontageEndedInternal = FOnMontageEnded::CreateUObject(this, &UAbilityTask_PlayMontageWithEvents::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(OnMontageEndedInternal, MontageToPlay);
    DEBUG_LOG(TEXT("MontageEndedDelegate Bound Successfully, Montage Name: %s") ,MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL"));
}

void UAbilityTask_PlayMontageWithEvents::UnbindMontageCallbacks()
{
    const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
    if (!ActorInfo)
    {
        return;
    }

    UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
    if (AnimInstance && MontageToPlay)
    {
        if (OnBlendingOutInternal.IsBound())
        {
            FOnMontageBlendingOutStarted EmptyBlendDelegate;
            AnimInstance->Montage_SetBlendingOutDelegate(EmptyBlendDelegate, MontageToPlay);
            OnBlendingOutInternal.Unbind();
            DEBUG_LOG(TEXT("BlendingOutDelegate Unbound Successfully, Montage Name: %s") ,MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL"));
        }
        
        if (OnMontageEndedInternal.IsBound())
        {
            FOnMontageEnded EmptyEndDelegate;
            AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, MontageToPlay);
            OnMontageEndedInternal.Unbind();
            DEBUG_LOG(TEXT("MontageEndedDelegate Unbound Successfully, Montage Name: %s") ,MontageToPlay ? *MontageToPlay->GetName() : TEXT("NULL"));
        }
    }
}
#pragma endregion

void UAbilityTask_PlayMontageWithEvents::OnDestroy(bool AbilityEnded)
{
    DEBUG_LOG(TEXT("Montage With Events Task Destroyed"));

    if (bStopMontageWhenAbilityCancelled)
    {
        StopPlayingMontage();
    }

    //이벤트 콜백 해제
    UnbindAllEventCallbacks();

    //몽타주 델리게이트 해제
    UnbindMontageCallbacks();

    //커브 폴링 정리
    bUseCurvePolling = false;
    bTickingTask = false;

    bStopMontageWhenAbilityCancelled = false;

    MontageToPlay = nullptr;

    Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_PlayMontageWithEvents::ExternalCancel()
{
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnTaskCancelled.Broadcast();
    }

    Super::ExternalCancel();
}