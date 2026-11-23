#include "AI/StateTree/Conditions/TargetAngleCondition.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTargetAngleCondition, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogTargetAngleCondition, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool FTargetAngleCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bWithinAngle = FMath::Abs(InstanceData.AngleToTarget) <= InstanceData.MaxAngle;

	DEBUG_LOG(TEXT("TestCondition: Angle=%.1f, Max=%.1f, Within=%d, Invert=%d"), InstanceData.AngleToTarget, InstanceData.MaxAngle, bWithinAngle, InstanceData.bInvert);

	return InstanceData.bInvert ? !bWithinAngle : bWithinAngle;
}

#if WITH_EDITOR
FText FTargetAngleCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		const TCHAR* OperatorStr = InstanceData->bInvert ? TEXT(">") : TEXT("<=");

		return FText::FromString(FString::Printf(TEXT("<b>|Target Angle| %s</b> %.0f"), OperatorStr, InstanceData->MaxAngle));
	}

	return FText::FromString(TEXT("<b>Target Angle</b>"));
}
#endif //WITH_EDITOR
