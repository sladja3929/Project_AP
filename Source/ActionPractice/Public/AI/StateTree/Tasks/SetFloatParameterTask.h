#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "SetFloatParameterTask.generated.h"

USTRUCT()
struct FSetFloatParameterTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//설정할 목표 값 (상수 직접 입력 또는 파라미터 바인딩)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float NewValue = 0.0f;

	//Output: 전역 파라미터에 바인딩할 출력 값
	UPROPERTY(EditAnywhere, Category = "Output")
	float OutputValue = 0.0f;

#pragma endregion
};

/**
 * 전역 파라미터(float)의 값을 변경하는 즉발형 Task
 * - EnterState에서 NewValue → OutputValue 복사 후 Succeeded 반환
 * - 에디터에서 OutputValue를 전역 파라미터에 바인딩하여 사용
 */
USTRUCT(meta = (DisplayName = "Set Float Parameter", Category = "Parameter"))
struct ACTIONPRACTICE_API FSetFloatParameterTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FSetFloatParameterTaskInstanceData;

	FSetFloatParameterTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion
};
