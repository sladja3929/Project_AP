#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "TargetAngleCondition.generated.h"

USTRUCT()
struct FTargetAngleConditionInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//정면 기준 타깃 각도 (부호 있음, 음수 왼쪽 / 양수 오른쪽). Update Target Info Evaluator의 AngleToTarget을 바인딩
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AngleToTarget = 0.0f;

	//허용 최대 각도 (절대값 기준)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAngle = 60.0f;

	//결과 반전 (true면 각도가 MaxAngle을 넘을 때 통과)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

#pragma endregion
};

/**
 * 타깃이 정면 기준 MaxAngle 안에 있는지 검사하는 Condition (|AngleToTarget| <= MaxAngle)
 * 몽타주 행동(공격 어빌리티, 연출 몽타주)의 시작 각도 게이트. 각도를 벗어나 행동 그룹이 선택되지 않으면
 * 뒤쪽의 Turn To Target 상태가 선택되어 회전 후 재선택
 */
USTRUCT(meta = (DisplayName = "Target Angle", Category = "AI"))
struct ACTIONPRACTICE_API FTargetAngleCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FTargetAngleConditionInstanceData;

	FTargetAngleCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion
};
