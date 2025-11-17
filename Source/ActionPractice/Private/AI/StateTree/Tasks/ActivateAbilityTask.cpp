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
    InstanceData.bAbilityEnded = false;
    InstanceData.bAbilityCancelled = false;
    
    if (!InstanceData.AbilitySystemComponent)
    {
        DEBUG_LOG(TEXT("AbilitySystemComponent is nullptr"));
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.AbilityToActivate)
    {
        DEBUG_LOG(TEXT("AbilityToActivate is nullptr"));
        return EStateTreeRunStatus::Failed;
    }
    
    FGameplayAbilitySpec* Spec = InstanceData.AbilitySystemComponent->FindAbilitySpecFromClass(InstanceData.AbilityToActivate);

    if (!Spec)
    {
        DEBUG_LOG(TEXT("No AbilitySpec: %s"), *GetNameSafe(InstanceData.AbilityToActivate));
        return EStateTreeRunStatus::Failed;
    }
    
    InstanceData.AbilityHandle = Spec->Handle;
    DEBUG_LOG(TEXT("Using Ability: %s"), *InstanceData.AbilityToActivate->GetName());

    //Task는 UStruct라서 ASC의 어빌리티 종료 델리게이트(인자 1개)에 콜백 함수를 바인딩하지 못함, 람다 함수를 통해 플래그만 받고 Tick에서 확인
    //AddDynamic: UFUNCTION 사용 불가, AddUObject: UObject가 아님, AddRaw: GC 등록이 되지 않아 안전하지 않음, 구조체 객체 자체도 위험
    InstanceData.AbilityEndedHandle = InstanceData.AbilitySystemComponent->OnAbilityEnded.AddLambda(
        [&InstanceData, AbilityHandle = InstanceData.AbilityHandle](const FAbilityEndedData& EndData)
        {
            //종료된 어빌리티 핸들이 태스크가 생성한 것인지 확인
            if (EndData.AbilitySpecHandle == AbilityHandle)
            {
                InstanceData.bAbilityEnded = true;
                InstanceData.bAbilityCancelled = EndData.bWasCancelled;
                if (EndData.bWasCancelled)
                {
                    DEBUG_LOG(TEXT("Ability cancelled In STTask"));
                }
                
                else
                {
                    DEBUG_LOG(TEXT("Ability completed normally In STTask"));
                }
            }
        });

    //어빌리티 활성화
    const bool bSuccess = InstanceData.AbilitySystemComponent->TryActivateAbility(InstanceData.AbilityHandle, true);
    if (!bSuccess)
    {
        DEBUG_LOG(TEXT("Failed to activate ability: %s"), *InstanceData.AbilityToActivate->GetName());
        return EStateTreeRunStatus::Failed;
    }

    DEBUG_LOG(TEXT("Successfully activated ability: %s"), *InstanceData.AbilityToActivate->GetName());
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FActivateAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    //어빌리티가 끝났는지 확인
    if (InstanceData.bAbilityEnded)
    {
        if (InstanceData.bAbilityCancelled)
        {
            DEBUG_LOG(TEXT("Task failed - Ability was cancelled"));
            return EStateTreeRunStatus::Failed;
        }
        else
        {
            DEBUG_LOG(TEXT("Task succeeded - Ability completed"));
            return EStateTreeRunStatus::Succeeded;
        }
    }

    return EStateTreeRunStatus::Running;
}

void FActivateAbilityTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    //델리게이트 언바인딩
    if (InstanceData.AbilitySystemComponent && InstanceData.AbilityEndedHandle.IsValid())
    {
        InstanceData.AbilitySystemComponent->OnAbilityEnded.Remove(InstanceData.AbilityEndedHandle);
        InstanceData.AbilityEndedHandle.Reset();
    }

    //어빌리티가 아직 실행 중이면 취소
    if (InstanceData.AbilitySystemComponent && InstanceData.AbilityHandle.IsValid())
    {
        InstanceData.AbilitySystemComponent->CancelAbilityHandle(InstanceData.AbilityHandle);
        DEBUG_LOG(TEXT("Cancelled ability on exit"));
    }

    DEBUG_LOG(TEXT("Exit Task"));
}
