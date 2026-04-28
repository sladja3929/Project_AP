#include "AI/StateTree/Tasks/ApplyGameplayEffectTask.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "StateTreeExecutionContext.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogApplyGameplayEffectTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogApplyGameplayEffectTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FApplyGameplayEffectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAbilitySystemComponent)
	{
		DEBUG_LOG(TEXT("EnterState: ASC is nullptr"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.EffectClass)
	{
		DEBUG_LOG(TEXT("EnterState: EffectClass is nullptr"));
		return EStateTreeRunStatus::Failed;
	}

	//GE Spec 생성
	FGameplayEffectContextHandle EffectContext = InstanceData.EnemyAbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = InstanceData.EnemyAbilitySystemComponent->MakeOutgoingSpec(
		InstanceData.EffectClass, 1.0f, EffectContext);

	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("EnterState: Failed to create GE Spec. EffectClass=%s"),
			*GetNameSafe(InstanceData.EffectClass));
		return EStateTreeRunStatus::Failed;
	}

	//SetByCaller: Magnitude
	const FGameplayTag& MagnitudeTag = UGameplayTagsSubsystem::GetEffectStateTreeMagnitudeTag();
	if (MagnitudeTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(MagnitudeTag, InstanceData.Magnitude);
		DEBUG_LOG(TEXT("EnterState: SetByCaller Magnitude=%.2f, Tag=%s"),
			InstanceData.Magnitude, *MagnitudeTag.ToString());
	}

	//SetByCaller: Duration (0보다 클 때만 주입)
	if (InstanceData.Duration > 0.0f)
	{
		const FGameplayTag& DurationTag = UGameplayTagsSubsystem::GetEffectStateTreeDurationTag();
		if (DurationTag.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(DurationTag, InstanceData.Duration);
			DEBUG_LOG(TEXT("EnterState: SetByCaller Duration=%.2f, Tag=%s"),
				InstanceData.Duration, *DurationTag.ToString());
		}
	}

	//GE 적용
	InstanceData.ActiveEffectHandle = InstanceData.EnemyAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*SpecHandle.Data.Get());

	if (!InstanceData.ActiveEffectHandle.IsValid())
	{
		DEBUG_LOG(TEXT("EnterState: ApplyGameplayEffectSpecToSelf FAILED. EffectClass=%s"),
			*GetNameSafe(InstanceData.EffectClass));
		return EStateTreeRunStatus::Failed;
	}

	DEBUG_LOG(TEXT("EnterState: GE Applied. EffectClass=%s, bEndWithState=%s"),
		*GetNameSafe(InstanceData.EffectClass),
		InstanceData.bEndWithState ? TEXT("true") : TEXT("false"));

	return EStateTreeRunStatus::Running;
}

void FApplyGameplayEffectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.bEndWithState && InstanceData.ActiveEffectHandle.IsValid())
	{
		if (InstanceData.EnemyAbilitySystemComponent)
		{
			InstanceData.EnemyAbilitySystemComponent->RemoveActiveGameplayEffect(InstanceData.ActiveEffectHandle);
			DEBUG_LOG(TEXT("ExitState: GE Removed. EffectClass=%s"),
				*GetNameSafe(InstanceData.EffectClass));
		}
	}

	InstanceData.ActiveEffectHandle = FActiveGameplayEffectHandle();
}

#if WITH_EDITOR
FText FApplyGameplayEffectTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData && InstanceData->EffectClass)
	{
		return FText::FromString(FString::Printf(TEXT("<b>Apply GE:</b> %s"), *InstanceData->EffectClass->GetName()));
	}

	return FText::FromString(TEXT("<b>Apply GE:</b> None"));
}
#endif //WITH_EDITOR
