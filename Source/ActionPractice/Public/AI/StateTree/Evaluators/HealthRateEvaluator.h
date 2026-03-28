#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "HealthRateEvaluator.generated.h"

class UEnemyAbilitySystemComponent;
class UAbilitySystemComponent;

/**
 * Evaluator Instance Data
 * Context: 스키마에서 설정한 Context에서 자동으로 같은 자료형을 찾아 바인딩
 * Input/Parameter: 에디터에서 설정하는 입력 값 (직접 입력 or 다른 노드의 Output 연결)
 * Output: 노드의 결과 값
 */
USTRUCT()
struct ACTIONPRACTICE_API FHealthRateEvaluatorInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UEnemyAbilitySystemComponent> EnemyAbilitySystemComponent = nullptr;

	//Output: 현재 체력 비율 (0.0 ~ 1.0)
	UPROPERTY(EditAnywhere, Category = "Output")
	float HealthRate = 1.0f;
};

/**
 * ASC AttributeSet의 현재 체력 비율을 계산하는 Evaluator
 */
USTRUCT()
struct ACTIONPRACTICE_API FHealthRateEvaluator : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FHealthRateEvaluatorInstanceData;

	FHealthRateEvaluator() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

protected:

	void UpdateHealthRate(FStateTreeExecutionContext& Context) const;
};