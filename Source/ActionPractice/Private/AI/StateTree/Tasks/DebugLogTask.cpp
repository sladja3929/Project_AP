#include "AI/StateTree/Tasks/DebugLogTask.h"
#include "StateTreeExecutionContext.h"
#include "GameFramework/Actor.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogDebugLogTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogDebugLogTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

FDebugLogTask::FDebugLogTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FDebugLogTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	DEBUG_LOG(TEXT("%s %s: V1=%.1f V2=%.1f V3=%.2f V4=%.2f"),
		*InstanceData.Message, *GetNameSafe(InstanceData.Actor), InstanceData.Value1, InstanceData.Value2, InstanceData.Value3, InstanceData.Value4);

	return EStateTreeRunStatus::Running;
}
