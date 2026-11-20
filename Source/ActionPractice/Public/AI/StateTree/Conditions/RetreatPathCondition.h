#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "RetreatPathCondition.generated.h"

class AEnemyAIController;

USTRUCT()
struct FRetreatPathConditionInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context에 바인딩된 AI 컨트롤러 (폰·타깃 조회용)
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//타깃 반대 방향으로 물러날 수평 거리 (후퇴 몽타주의 루트 모션 이동량 이상)
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float RetreatDistance = 300.0f;

#pragma endregion
};

/**
 * 타깃 반대 방향으로 RetreatDistance만큼 내비메시가 끊기지 않고 이어지는지 검사하는 Condition
 * 백점프 등 후퇴 몽타주가 절벽·턱 아래로 떨어지거나 벽에 막히는 것을 방지
 * 내비메시 레이캐스트는 메시 경계(낭떠러지, 연결되지 않은 다른 층, 장애물)에서 막히므로 같은 층으로 이어진 경로만 통과
 * 타깃이 없으면 정면 반대 방향 기준
 */
USTRUCT(meta = (DisplayName = "Retreat Path Clear", Category = "AI"))
struct ACTIONPRACTICE_API FRetreatPathCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FRetreatPathConditionInstanceData;

	FRetreatPathCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion
};
