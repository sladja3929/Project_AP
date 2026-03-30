#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class FComboAttackUnitCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	//부모 구조체(FTaggedAttackData 또는 FEnemyTaggedAttackData)의 AttackType PropertyHandle을 찾는다
	//PropertyHandle 체인을 최대 MaxDepth 레벨까지 역추적하며 "AttackType" 자식을 탐색
	static TSharedPtr<IPropertyHandle> FindParentAttackTypeHandle(TSharedRef<IPropertyHandle> StructPropertyHandle, int32 MaxDepth = 5);
};
