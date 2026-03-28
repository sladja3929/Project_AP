#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "GameplayTagContainer.h"
#include "HasGameplayTagsCondition.generated.h"

class UEnemyAbilitySystemComponent;

//매칭 모드
UENUM()
enum class EGameplayTagMatchMode : uint8
{
	//모든 태그가 존재해야 통과
	All UMETA(DisplayName = "Has All"),

	//하나라도 존재하면 통과
	Any UMETA(DisplayName = "Has Any"),
};

USTRUCT()
struct FHasGameplayTagsConditionInstanceData
{
	GENERATED_BODY()

#pragma region "Public Variables"

	//Context에 바인딩된 ASC
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UEnemyAbilitySystemComponent> EnemyAbilitySystemComponent = nullptr;

	//체크할 태그 컨테이너
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer Tags;

	//매칭 모드 (All: 모든 태그 필요, Any: 하나라도 있으면 통과)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EGameplayTagMatchMode MatchMode = EGameplayTagMatchMode::All;

	//결과 반전 (true면 조건 불만족 시 통과)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

#pragma endregion
};

/**
 * ASC의 OwnedGameplayTags에서 지정된 태그 컨테이너를 All/Any 모드로 검사하는 Condition
 */
USTRUCT(meta = (DisplayName = "Has Gameplay Tags", Category = "GAS"))
struct ACTIONPRACTICE_API FHasGameplayTagsCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

#pragma region "Public Functions"

	using FInstanceDataType = FHasGameplayTagsConditionInstanceData;

	FHasGameplayTagsCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif //WITH_EDITOR

#pragma endregion
};
