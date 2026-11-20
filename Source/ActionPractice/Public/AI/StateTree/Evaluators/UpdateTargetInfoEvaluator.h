#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "UpdateTargetInfoEvaluator.generated.h"

class AActor;
class AEnemyAIController;
class AEnemyCharacter;

/**
 * Evaluator Instance Data
 * Context: 스키마에서 설정한 Context에서 자동으로 같은 자료형을 찾아 바인딩
 * Input/Parameter: 에디터에서 설정하는 입력 값 (직접 입력 or 다른 노드의 Output 연결)
 * Output: 노드의 결과 값
 */
USTRUCT()
struct ACTIONPRACTICE_API FUpdateTargetInfoEvaluatorInstanceData
{
	GENERATED_BODY()

	//Context: EnemyAIController
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//Context: EnemyCharacter
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyCharacter> EnemyCharacter = nullptr;

	//Output: 타겟 액터
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> DetectedTarget = nullptr;

	//Output: 타겟까지의 수평 거리 (액터 중심 간, 높이 차 제외)
	UPROPERTY(EditAnywhere, Category = "Output")
	float DistanceToTarget = -1.0f;

	//Output: Enemy의 정면 기준 Target과의 수평 각도 (Yaw, -180~180, 음수 왼쪽 / 양수 오른쪽)
	UPROPERTY(EditAnywhere, Category = "Output")
	float AngleToTarget = 0.0f;

	//Output: 타깃 보유 여부 (인지 또는 전투)
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bTargetDetected  = false;

	//Output: 전투 상태 여부 (EnemyAIController 소유. 전투 중에는 시야를 벗어나도 타깃 유지)
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bInCombat = false;
};

/**
 * AIPerception에서 감지된 타겟의 정보(거리, 각도)를 계산하고 갱신하는 Evaluator
 */
USTRUCT()
struct ACTIONPRACTICE_API FUpdateTargetInfoEvaluator : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()

	using FInstanceDataType = FUpdateTargetInfoEvaluatorInstanceData;

	FUpdateTargetInfoEvaluator() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

protected:

	void UpdateTargetInfo(FStateTreeExecutionContext& Context) const;
};