#include "Customization/TaggedAttackDataCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "GameplayTagContainer.h"
#include "Items/WeaponDataAsset.h"
#include "Items/AttackData.h"

TSharedRef<IPropertyTypeCustomization> FTaggedAttackDataCustomization::MakeInstance()
{
	return MakeShareable(new FTaggedAttackDataCustomization());
}

void FTaggedAttackDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> AttackTagsHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTaggedAttackData, AttackTags));
	TSharedPtr<IPropertyHandle> ComboSequenceHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTaggedAttackData, ComboSequence));
	TSharedPtr<IPropertyHandle> AttackTypeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTaggedAttackData, AttackType));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(350.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(IPropertyTypeCustomizationUtils::GetBoldFont())
			.Text_Lambda([AttackTagsHandle]() -> FText
			{
				if (!AttackTagsHandle.IsValid()) return FText::FromString(TEXT("(No Tags)"));

				FGameplayTagContainer Tags;
				void* ValuePtr = nullptr;
				if (AttackTagsHandle->GetValueData(ValuePtr) == FPropertyAccess::Success && ValuePtr)
				{
					Tags = *static_cast<FGameplayTagContainer*>(ValuePtr);
				}

				FString TagStr = Tags.IsEmpty() ? TEXT("(No Tags)") : Tags.ToStringSimple();
				return FText::FromString(FString::Printf(TEXT("[%s]"), *TagStr));
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([ComboSequenceHandle]() -> FText
			{
				uint32 NumChildren = 0;
				if (ComboSequenceHandle.IsValid())
				{
					ComboSequenceHandle->GetNumChildren(NumChildren);
				}
				return FText::FromString(FString::Printf(TEXT("%d Combos"), NumChildren));
			})
		]

		//AttackType 표시
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 0.4f)))
			.Text_Lambda([AttackTypeHandle]() -> FText
			{
				if (!AttackTypeHandle.IsValid()) return FText::GetEmpty();
				uint8 Val = 0;
				AttackTypeHandle->GetValue(Val);
				if (static_cast<EComboAttackType>(Val) == EComboAttackType::Charge)
				{
					return FText::FromString(TEXT("| Charge"));
				}
				return FText::GetEmpty(); //Normal은 표시하지 않음 (기본이므로)
			})
		]
	];
}

void FTaggedAttackDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(i).ToSharedRef();
		StructBuilder.AddProperty(ChildHandle);
	}
}
