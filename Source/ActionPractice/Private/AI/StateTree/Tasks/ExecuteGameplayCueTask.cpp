#include "AI/StateTree/Tasks/ExecuteGameplayCueTask.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogExecuteGameplayCueTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogExecuteGameplayCueTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

FExecuteGameplayCueTask::FExecuteGameplayCueTask()
{
	//항상 Running인 연출 태스크라 상태 완료 판정에서 제외
	bConsideredForCompletion = false;
}

EStateTreeRunStatus FExecuteGameplayCueTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.ElapsedTime = 0.0f;
	InstanceData.NextExecuteTime = RandomWithDeviation(InstanceData.FirstDelay, InstanceData.FirstDelayDeviation);

	if (!InstanceData.EnemyAbilitySystemComponent || !InstanceData.GameplayCueTag.IsValid())
	{
		DEBUG_LOG(TEXT("EnterState: ASC or GameplayCueTag is invalid, cue will not be executed"));
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FExecuteGameplayCueTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAbilitySystemComponent || !InstanceData.GameplayCueTag.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime < InstanceData.NextExecuteTime)
	{
		return EStateTreeRunStatus::Running;
	}

	//서버에서 실행하면 ASC가 클라이언트로 멀티캐스트
	InstanceData.EnemyAbilitySystemComponent->ExecuteGameplayCue(InstanceData.GameplayCueTag, InstanceData.EnemyAbilitySystemComponent->MakeEffectContext());
	DEBUG_LOG(TEXT("Tick: Executed %s at %.2fs"), *InstanceData.GameplayCueTag.ToString(), InstanceData.ElapsedTime);

	if (InstanceData.bRepeat)
	{
		//간격이 0에 가까워도 한 틱에 여러 번 실행되지 않도록 현재 시각 기준으로 다음 실행 예약
		InstanceData.NextExecuteTime = InstanceData.ElapsedTime + FMath::Max(0.1f, RandomWithDeviation(InstanceData.Interval, InstanceData.IntervalDeviation));
	}
	else
	{
		InstanceData.NextExecuteTime = TNumericLimits<float>::Max();
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FExecuteGameplayCueTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		if (InstanceData->bRepeat)
		{
			return FText::FromString(FString::Printf(TEXT("<b>Execute Gameplay Cue</b> %s (first %.1f±%.1fs, every %.1f±%.1fs)"),
				*InstanceData->GameplayCueTag.ToString(), InstanceData->FirstDelay, InstanceData->FirstDelayDeviation, InstanceData->Interval, InstanceData->IntervalDeviation));
		}

		return FText::FromString(FString::Printf(TEXT("<b>Execute Gameplay Cue</b> %s (once after %.1f±%.1fs)"),
			*InstanceData->GameplayCueTag.ToString(), InstanceData->FirstDelay, InstanceData->FirstDelayDeviation));
	}

	return FText::FromString(TEXT("<b>Execute Gameplay Cue</b>"));
}
#endif //WITH_EDITOR

float FExecuteGameplayCueTask::RandomWithDeviation(float Base, float Deviation)
{
	return FMath::Max(0.0f, Base + FMath::FRandRange(-Deviation, Deviation));
}
