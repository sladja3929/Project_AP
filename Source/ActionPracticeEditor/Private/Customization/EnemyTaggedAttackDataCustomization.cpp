#include "Customization/EnemyTaggedAttackDataCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "GameplayTagContainer.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Items/AttackData.h"

TSharedRef<IPropertyTypeCustomization> FEnemyTaggedAttackDataCustomization::MakeInstance()
{
	return MakeShareable(new FEnemyTaggedAttackDataCustomization());
}

void FEnemyTaggedAttackDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> AttackTagsHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyTaggedAttackData, AttackTags));
	TSharedPtr<IPropertyHandle> ComboSequenceHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyTaggedAttackData, ComboSequence));
	TSharedPtr<IPropertyHandle> CooldownHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyTaggedAttackData, CooldownDuration));
	TSharedPtr<IPropertyHandle> AttackTypeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyTaggedAttackData, AttackType));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(400.0f)
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

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.6f, 0.2f)))
			.Text_Lambda([CooldownHandle]() -> FText
			{
				float CooldownValue = 0.0f;
				if (CooldownHandle.IsValid())
				{
					CooldownHandle->GetValue(CooldownValue);
				}
				if (CooldownValue > 0.0f)
				{
					return FText::FromString(FString::Printf(TEXT("| CD: %.1fs"), CooldownValue));
				}
				return FText::GetEmpty();
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
				const EComboAttackType Type = static_cast<EComboAttackType>(Val);
				if (Type == EComboAttackType::Charge)
				{
					return FText::FromString(TEXT("| Charge"));
				}
				if (Type == EComboAttackType::Lunge)
				{
					return FText::FromString(TEXT("| Lunge"));
				}
				return FText::GetEmpty();
			})
		]
	];
}

void FEnemyTaggedAttackDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(i).ToSharedRef();
		StructBuilder.AddProperty(ChildHandle);
	}
}
