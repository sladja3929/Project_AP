#include "AI/StateTree/Tasks/PlayMontageTask.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimMontage.h"
#include "Characters/BaseCharacter.h"
#include "Characters/APMotionWarpingComponent.h"
#include "AI/EnemyAIController.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
    DEFINE_LOG_CATEGORY_STATIC(LogPlayMontageTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogPlayMontageTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

namespace
{
    UAPMotionWarpingComponent* GetMotionWarpingComponent(const UAbilitySystemComponent* ASC)
    {
        const ABaseCharacter* Character = ASC ? Cast<ABaseCharacter>(ASC->GetAvatarActor()) : nullptr;
        return Character ? Character->GetMotionWarpingComponent() : nullptr;
    }
}

EStateTreeRunStatus FPlayMontageTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    InstanceData.ElapsedTime = 0.0f;
    InstanceData.bEndRequested = false;
    InstanceData.bWarpTargetSet = false;
    InstanceData.ComputedDuration = 0.0f;

    if (InstanceData.Duration > 0.0f)
    {
        InstanceData.ComputedDuration = FMath::Max(0.0f, InstanceData.Duration + FMath::FRandRange(-InstanceData.RandomDeviation, InstanceData.RandomDeviation));
    }

    UEnemyAbilitySystemComponent* ASC = InstanceData.EnemyAbilitySystemComponent;

    if (!ASC || !InstanceData.Montage)
    {
        DEBUG_LOG(TEXT("EnterState: ASC(%p) or Montage(%s) is null"), ASC, *GetNameSafe(InstanceData.Montage));
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.EndSection.IsNone())
    {
        const int32 EndSectionIndex = InstanceData.Montage->GetSectionIndex(InstanceData.EndSection);

        if (EndSectionIndex == INDEX_NONE)
        {
            DEBUG_LOG(TEXT("EnterState: EndSection %s not found in %s"), *InstanceData.EndSection.ToString(), *GetNameSafe(InstanceData.Montage));
            return EStateTreeRunStatus::Failed;
        }

        //EndSection이 다른 섹션으로 이어지면(루프 포함) 몽타주가 끝나지 않음
        if (!InstanceData.Montage->CompositeSections[EndSectionIndex].NextSectionName.IsNone())
        {
            DEBUG_LOG(TEXT("EnterState: EndSection %s of %s must have no next section"), *InstanceData.EndSection.ToString(), *GetNameSafe(InstanceData.Montage));
            return EStateTreeRunStatus::Failed;
        }
    }

    //몽타주 시작 전에 워프 타깃을 세팅해야 첫 워프 윈도우부터 반영됨
    if (!InstanceData.WarpTargetName.IsNone())
    {
        if (UAPMotionWarpingComponent* MotionWarpingComponent = GetMotionWarpingComponent(ASC))
        {
            const AActor* TargetActor = nullptr;

            if (InstanceData.EnemyAIController)
            {
                const FCurrentTarget& TargetInfo = InstanceData.EnemyAIController->GetCurrentTarget();
                TargetActor = TargetInfo.IsValid() ? TargetInfo.Actor.Get() : nullptr;
            }

            MotionWarpingComponent->SetWarpTargetFollowActor(InstanceData.WarpTargetName, TargetActor);
            InstanceData.bWarpTargetSet = true;
        }
    }

    //어빌리티 없이 ASC로 재생 (AnimatingAbility는 비워 두고 RepAnimMontageInfo 복제만 이용)
    const float PlayLength = ASC->PlayMontage(nullptr, FGameplayAbilityActivationInfo(), InstanceData.Montage, InstanceData.PlayRate, InstanceData.StartSection);

    if (PlayLength <= 0.0f)
    {
        DEBUG_LOG(TEXT("EnterState: PlayMontage failed. Montage=%s"), *GetNameSafe(InstanceData.Montage));
        return EStateTreeRunStatus::Failed;
    }

    DEBUG_LOG(TEXT("EnterState: Playing %s, Duration=%.2f"), *GetNameSafe(InstanceData.Montage), InstanceData.ComputedDuration);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPlayMontageTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
    UEnemyAbilitySystemComponent* ASC = InstanceData.EnemyAbilitySystemComponent;

    if (!ASC)
    {
        return EStateTreeRunStatus::Failed;
    }

    //몽타주 종료 또는 다른 몽타주(피격 어빌리티 등)로 대체
    if (ASC->GetCurrentMontage() != InstanceData.Montage)
    {
        DEBUG_LOG(TEXT("Tick: %s is no longer playing - Task SUCCEEDED"), *GetNameSafe(InstanceData.Montage));
        return EStateTreeRunStatus::Succeeded;
    }

    if (InstanceData.bEndRequested || InstanceData.ComputedDuration <= 0.0f)
    {
        return EStateTreeRunStatus::Running;
    }

    InstanceData.ElapsedTime += DeltaTime;

    if (InstanceData.ElapsedTime < InstanceData.ComputedDuration)
    {
        return EStateTreeRunStatus::Running;
    }

    InstanceData.bEndRequested = true;

    if (InstanceData.EndSection.IsNone())
    {
        ASC->CurrentMontageStop();
        DEBUG_LOG(TEXT("Tick: Duration elapsed, stopped %s - Task SUCCEEDED"), *GetNameSafe(InstanceData.Montage));
        return EStateTreeRunStatus::Succeeded;
    }

    //즉시 점프 대신 현재 섹션이 끝난 뒤 EndSection으로 이어지도록 Next Section만 교체 (포즈 튐 방지)
    const FName CurrentSection = ASC->GetCurrentMontageSectionName();

    if (CurrentSection != InstanceData.EndSection)
    {
        ASC->CurrentMontageSetNextSectionName(CurrentSection, InstanceData.EndSection);
    }

    DEBUG_LOG(TEXT("Tick: Duration elapsed, %s -> %s"), *CurrentSection.ToString(), *InstanceData.EndSection.ToString());
    return EStateTreeRunStatus::Running;
}

void FPlayMontageTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
    UEnemyAbilitySystemComponent* ASC = InstanceData.EnemyAbilitySystemComponent;

    //내가 재생한 몽타주가 아직 재생 중일 때만 정지
    if (ASC && InstanceData.Montage && ASC->GetCurrentMontage() == InstanceData.Montage)
    {
        ASC->CurrentMontageStop();
        DEBUG_LOG(TEXT("ExitState: Stopped %s"), *GetNameSafe(InstanceData.Montage));
    }

    //직접 세팅한 워프 타깃만 정리 (검증 실패로 세팅 전 종료된 경우 다른 소유자의 타깃을 지우지 않음)
    if (InstanceData.bWarpTargetSet)
    {
        if (UAPMotionWarpingComponent* MotionWarpingComponent = GetMotionWarpingComponent(ASC))
        {
            MotionWarpingComponent->RemoveWarpTarget(InstanceData.WarpTargetName);
        }

        InstanceData.bWarpTargetSet = false;
    }
}
