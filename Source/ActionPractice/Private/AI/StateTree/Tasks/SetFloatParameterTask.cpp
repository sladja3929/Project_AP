#include "AI/StateTree/Tasks/SetFloatParameterTask.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogSetFloatParameterTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogSetFloatParameterTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FSetFloatParameterTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.OutputValue = InstanceData.NewValue;

	DEBUG_LOG(TEXT("EnterState: OutputValue set to %.2f"), InstanceData.OutputValue);

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FSetFloatParameterTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Set Float:</b> %.2f"), InstanceData->NewValue));
	}

	return FText::FromString(TEXT("<b>Set Float Parameter</b>"));
}
#endif //WITH_EDITOR
