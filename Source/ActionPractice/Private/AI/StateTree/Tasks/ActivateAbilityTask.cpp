#include "AI/StateTree/Tasks/ActivateAbilityTask.h"
#include "AbilitySystemComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
    DEFINE_LOG_CATEGORY_STATIC(LogActivateAbilityTask, Log, All);
    #define DEBUG_LOG(Format, ...) UE_LOG(LogActivateAbilityTask, Warning, Format, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FActivateAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    DEBUG_LOG(TEXT("EnterState: AbilityTask started. InstanceData=%p"), &InstanceData);

    InstanceData.bAbilityEnded = false;
    InstanceData.bAbilityCancelled = false;
    InstanceData.ElapsedEndDelay = 0.0f;

    if (!InstanceData.AbilitySystemComponent)
    {
        DEBUG_LOG(TEXT("EnterState: ASC is nullptr"));
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.AbilityToActivate)
    {
        DEBUG_LOG(TEXT("EnterState: AbilityToActivate is nullptr"));
        return EStateTreeRunStatus::Failed;
    }
    
    FGameplayAbilitySpec* Spec = InstanceData.AbilitySystemComponent->FindAbilitySpecFromClass(InstanceData.AbilityToActivate);

    if (!Spec)
    {
        DEBUG_LOG(TEXT("EnterState: No AbilitySpec. Ability=%s, ASC=%p"),
            *GetNameSafe(InstanceData.AbilityToActivate),
            InstanceData.AbilitySystemComponent.Get());
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.AbilityHandle = Spec->Handle;

    DEBUG_LOG(TEXT("EnterState: Using Ability=%s, Handle=%s, ASC=%p"),
        *GetNameSafe(InstanceData.AbilityToActivate),
        *InstanceData.AbilityHandle.ToString(),
        InstanceData.AbilitySystemComponent.Get());

    //어빌리티 활성화
    const bool bSuccess = InstanceData.AbilitySystemComponent->TryActivateAbility(InstanceData.AbilityHandle, true);

    if (!bSuccess)
    {
        DEBUG_LOG(TEXT("EnterState: TryActivateAbility FAILED. Ability=%s, Handle=%s"),
            *GetNameSafe(InstanceData.AbilityToActivate),
            *InstanceData.AbilityHandle.ToString());
        return EStateTreeRunStatus::Failed;
    }

    DEBUG_LOG(TEXT("EnterState: TryActivateAbility SUCCEEDED. Ability=%s, Handle=%s"),
        *GetNameSafe(InstanceData.AbilityToActivate),
        *InstanceData.AbilityHandle.ToString());

    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FActivateAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    if (!InstanceData.AbilitySystemComponent || !InstanceData.AbilityHandle.IsValid())
    {
        DEBUG_LOG(TEXT("Tick: ASC or Handle invalid - Task FAILED"));
        return EStateTreeRunStatus::Failed;
    }

    //Spec 상태를 직접 확인
    const FGameplayAbilitySpec* Spec = InstanceData.AbilitySystemComponent->FindAbilitySpecFromHandle(InstanceData.AbilityHandle);

    if (!Spec)
    {
        DEBUG_LOG(TEXT("Tick: Spec is nullptr - Ability removed - Task FAILED"));
        return EStateTreeRunStatus::Failed;
    }

    //IsActive가 false이면 어빌리티가 끝난 것
    if (!Spec->IsActive())
    {
        //EndDelay 적용
        if (InstanceData.EndDelay > 0.0f)
        {
            InstanceData.ElapsedEndDelay += DeltaTime;

            if (InstanceData.ElapsedEndDelay >= InstanceData.EndDelay)
            {
                DEBUG_LOG(TEXT("Tick: Ability completed. EndDelay finished (%.2fs) - Task SUCCEEDED"), InstanceData.EndDelay);
                return EStateTreeRunStatus::Succeeded;
            }

            DEBUG_LOG(TEXT("Tick: Ability completed. Waiting for EndDelay (%.2f / %.2fs)"), InstanceData.ElapsedEndDelay, InstanceData.EndDelay);
            return EStateTreeRunStatus::Running;
        }
        else
        {
            DEBUG_LOG(TEXT("Tick: Ability is no longer active - Task SUCCEEDED"));
            return EStateTreeRunStatus::Succeeded;
        }
    }

    //아직 실행 중
    return EStateTreeRunStatus::Running;
}

void FActivateAbilityTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    DEBUG_LOG(TEXT("ExitState: Called. Ability=%s, Handle=%s, ASC=%p"),
        *GetNameSafe(InstanceData.AbilityToActivate),
        *InstanceData.AbilityHandle.ToString(),
        InstanceData.AbilitySystemComponent.Get());

    if (InstanceData.AbilitySystemComponent && InstanceData.AbilityHandle.IsValid())
    {
        //아직 활성화 중이면 취소
        InstanceData.AbilitySystemComponent->CancelAbilityHandle(InstanceData.AbilityHandle);
        DEBUG_LOG(TEXT("ExitState: Cancelled ability via CancelAbilityHandle"));
    }

    DEBUG_LOG(TEXT("ExitState: Finished."));
}
