#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "StateTreeTaskBase.h"
#include "ApplyGameplayEffectTask.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

USTRUCT()
struct FApplyGameplayEffectTaskInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context에 바인딩된 ASC
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	//적용할 GE 클래스
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UGameplayEffect> EffectClass = nullptr;

	//SetByCaller Magnitude 값 (Effect.StateTree.Magnitude 태그로 주입)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Magnitude = 0.0f;

	//SetByCaller Duration 값 (Effect.StateTree.Duration 태그로 주입, 0이면 GE 기본값 사용)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Duration = 0.0f;

	//true면 상태(State) 종료 시 GE를 강제 제거
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bEndWithState = true;

	//적용된 GE 핸들 (내부 관리용)
	FActiveGameplayEffectHandle ActiveEffectHandle;

#pragma endregion
};

/**
 * StateTree에서 GameplayEffect를 적용하는 범용 Task
 * 이동 속도 변경, 버프/디버프 등 GE 기반 어트리뷰트 조작에 사용
 */
USTRUCT(meta = (DisplayName = "Apply Gameplay Effect", Category = "GAS"))
struct ACTIONPRACTICE_API FApplyGameplayEffectTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FApplyGameplayEffectTaskInstanceData;

	FApplyGameplayEffectTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion
};
