#include "AI/StateTree/Tasks/WaitForTagRemovedTask.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWaitForTagRemovedTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWaitForTagRemovedTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FWaitForTagRemovedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAbilitySystemComponent || !InstanceData.Tag.IsValid())
	{
		DEBUG_LOG(TEXT("EnterState: ASC or Tag invalid — Failed"));
		return EStateTreeRunStatus::Failed;
	}

	//이미 태그가 없으면 즉시 완료
	if (!InstanceData.EnemyAbilitySystemComponent->HasMatchingGameplayTag(InstanceData.Tag))
	{
		DEBUG_LOG(TEXT("EnterState: Tag '%s' not present — Succeeded immediately"), *InstanceData.Tag.ToString());
		return EStateTreeRunStatus::Succeeded;
	}

	DEBUG_LOG(TEXT("EnterState: Waiting for tag '%s' removal"), *InstanceData.Tag.ToString());
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FWaitForTagRemovedTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAbilitySystemComponent)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.EnemyAbilitySystemComponent->HasMatchingGameplayTag(InstanceData.Tag))
	{
		DEBUG_LOG(TEXT("Tick: Tag '%s' removed — Succeeded"), *InstanceData.Tag.ToString());
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FWaitForTagRemovedTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData && InstanceData->Tag.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("<b>Wait Tag Removed:</b> %s"), *InstanceData->Tag.ToString()));
	}

	return FText::FromString(TEXT("<b>Wait For Tag Removed</b>"));
}
#endif //WITH_EDITOR
