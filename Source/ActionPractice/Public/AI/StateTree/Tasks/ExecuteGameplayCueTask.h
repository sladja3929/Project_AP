#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "ExecuteGameplayCueTask.generated.h"

class UEnemyAbilitySystemComponent;

USTRUCT()
struct FExecuteGameplayCueTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context에 바인딩된 ASC
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UEnemyAbilitySystemComponent> EnemyAbilitySystemComponent = nullptr;

	//실행할 GameplayCue 태그
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GameplayCue"))
	FGameplayTag GameplayCueTag;

	//상태 진입 후 첫 실행까지 시간 (초)과 랜덤 편차
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float FirstDelay = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float FirstDelayDeviation = 0.5f;

	//true면 상태에 머무는 동안 Interval마다 반복 실행
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bRepeat = true;

	//반복 간격 (초)과 랜덤 편차
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1", EditCondition = "bRepeat"))
	float Interval = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", EditCondition = "bRepeat"))
	float IntervalDeviation = 1.0f;

	float ElapsedTime = 0.0f;

	//다음 실행 시각 (상태 진입 기준 경과 시간)
	float NextExecuteTime = 0.0f;

#pragma endregion
};

/**
 * 상태에 머무는 동안 ASC에서 GameplayCue를 실행하는 Task (몽타주가 없는 상태의 울음·효과음 등)
 * - StateTree AI는 서버에서만 실행되므로 사운드를 직접 재생하지 않고 GameplayCue로 보내 클라이언트에 복제
 * - FirstDelay 후 첫 실행, bRepeat면 Interval마다 반복 (각각 ±Deviation 랜덤)
 * - 항상 Running으로 남고 상태 완료 판정에서 제외 (같은 상태의 다른 태스크가 완료를 결정)
 * - ASC나 태그가 없으면 실행만 건너뛰고 상태 진행은 막지 않음 (연출 전용)
 */
USTRUCT(meta = (DisplayName = "Execute Gameplay Cue", Category = "GAS"))
struct ACTIONPRACTICE_API FExecuteGameplayCueTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FExecuteGameplayCueTaskInstanceData;

	FExecuteGameplayCueTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion

private:
#pragma region "Private Functions"

	//기준값 ± 편차 랜덤 (0 미만은 0)
	static float RandomWithDeviation(float Base, float Deviation);

#pragma endregion
};
