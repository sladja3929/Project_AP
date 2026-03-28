#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "GameplayTagContainer.h"
#include "WaitForTagRemovedTask.generated.h"

class UEnemyAbilitySystemComponent;

USTRUCT()
struct FWaitForTagRemovedTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context에 바인딩된 ASC
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UEnemyAbilitySystemComponent> EnemyAbilitySystemComponent = nullptr;

	//대기할 태그 (이 태그가 ASC에서 제거되면 Succeeded)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;

#pragma endregion
};

/**
 * ASC에서 지정된 GameplayTag가 제거될 때까지 대기하는 Task
 * 태그가 존재하는 동안 Running, 제거되면 Succeeded 반환
 */
USTRUCT(meta = (DisplayName = "Wait For Tag Removed", Category = "GAS"))
struct ACTIONPRACTICE_API FWaitForTagRemovedTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FWaitForTagRemovedTaskInstanceData;

	FWaitForTagRemovedTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion
};
