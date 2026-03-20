#include "AI/StateTree/Conditions/HasGameplayTagsCondition.h"
#include "AbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogHasGameplayTagsCondition, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogHasGameplayTagsCondition, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool FHasGameplayTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AbilitySystemComponent || InstanceData.Tags.IsEmpty())
	{
		return InstanceData.bInvert;
	}

	bool bResult = false;

	switch (InstanceData.MatchMode)
	{
	case EGameplayTagMatchMode::All:
		bResult = InstanceData.AbilitySystemComponent->HasAllMatchingGameplayTags(InstanceData.Tags);
		break;

	case EGameplayTagMatchMode::Any:
		bResult = InstanceData.AbilitySystemComponent->HasAnyMatchingGameplayTags(InstanceData.Tags);
		break;
	}

	return InstanceData.bInvert ? !bResult : bResult;
}

#if WITH_EDITOR
FText FHasGameplayTagsCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();

	if (InstanceData)
	{
		const FString ModeStr = (InstanceData->MatchMode == EGameplayTagMatchMode::All) ? TEXT("All") : TEXT("Any");
		const FString InvertStr = InstanceData->bInvert ? TEXT("NOT ") : TEXT("");
		const FString TagStr = InstanceData->Tags.ToStringSimple();

		return FText::FromString(FString::Printf(TEXT("<b>%sHas %s:</b> %s"), *InvertStr, *ModeStr, *TagStr));
	}

	return FText::FromString(TEXT("<b>Has Gameplay Tags</b>"));
}
#endif //WITH_EDITOR
