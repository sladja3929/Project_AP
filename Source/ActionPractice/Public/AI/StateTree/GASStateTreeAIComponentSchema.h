#pragma once
#include "CoreMinimal.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "GASStateTreeAIComponentSchema.generated.h"

/*
 *EnemyAIController에서 사용하는 커스텀 State Tree Schema (기본 구조 설정)
 */

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "GAS AI Schema"))
class ACTIONPRACTICE_API UGASStateTreeAIComponentSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	UGASStateTreeAIComponentSchema();

	virtual void PostLoad() override;

	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

protected:
	virtual void SetContextData(FContextDataSetter& ContextDataSetter, bool bLogErrors) const override;
};
