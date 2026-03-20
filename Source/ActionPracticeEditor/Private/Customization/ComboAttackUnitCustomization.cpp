#include "Customization/ComboAttackUnitCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Items/AttackData.h"

TSharedRef<IPropertyTypeCustomization> FComboAttackUnitCustomization::MakeInstance()
{
	return MakeShareable(new FComboAttackUnitCustomization());
}

void FComboAttackUnitCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> MontageHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FComboAttackUnit, AttackMontage));
	TSharedPtr<IPropertyHandle> AttackDataHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FComboAttackUnit, AttackData));

	//AttackData 하위 프로퍼티
	TSharedPtr<IPropertyHandle> DamageTypeHandle;
	TSharedPtr<IPropertyHandle> DamageMultHandle;
	TSharedPtr<IPropertyHandle> PoiseDamageHandle;
	if (AttackDataHandle.IsValid())
	{
		DamageTypeHandle = AttackDataHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackStats, DamageType));
		DamageMultHandle = AttackDataHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackStats, DamageMultiplier));
		PoiseDamageHandle = AttackDataHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackStats, PoiseDamage));
	}

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(450.0f)
	[
		SNew(SHorizontalBox)

		//몽타주 이름
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(IPropertyTypeCustomizationUtils::GetBoldFont())
			.Text_Lambda([MontageHandle]() -> FText
			{
				if (!MontageHandle.IsValid()) return FText::FromString(TEXT("(None)"));

				FString MontageStr;
				MontageHandle->GetValueAsFormattedString(MontageStr);

				FString AssetName = MontageStr;
				int32 LastDot = INDEX_NONE;
				if (MontageStr.FindLastChar('.', LastDot))
				{
					AssetName = MontageStr.Mid(LastDot + 1);
					AssetName.RemoveFromEnd(TEXT("'"));
				}

				if (AssetName.IsEmpty() || AssetName == TEXT("None"))
				{
					return FText::FromString(TEXT("(No Montage)"));
				}
				return FText::FromString(AssetName);
			})
		]

		//DamageType x DamageMultiplier Poise:N
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			.Text_Lambda([DamageTypeHandle, DamageMultHandle, PoiseDamageHandle]() -> FText
			{
				//DamageType enum → 문자열
				FString DamageTypeStr = TEXT("None");
				if (DamageTypeHandle.IsValid())
				{
					uint8 EnumValue = 0;
					DamageTypeHandle->GetValue(EnumValue);
					switch (static_cast<EAttackDamageType>(EnumValue))
					{
					case EAttackDamageType::Slash:   DamageTypeStr = TEXT("Slash"); break;
					case EAttackDamageType::Strike:  DamageTypeStr = TEXT("Strike"); break;
					case EAttackDamageType::Pierce:  DamageTypeStr = TEXT("Pierce"); break;
					default: DamageTypeStr = TEXT("None"); break;
					}
				}

				float DamageMult = 1.0f;
				if (DamageMultHandle.IsValid()) DamageMultHandle->GetValue(DamageMult);

				float PoiseDmg = 0.0f;
				if (PoiseDamageHandle.IsValid()) PoiseDamageHandle->GetValue(PoiseDmg);

				return FText::FromString(FString::Printf(
					TEXT("| %s x%.1f  Poise:%.0f"), *DamageTypeStr, DamageMult, PoiseDmg));
			})
		]
	];
}

void FComboAttackUnitCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(i).ToSharedRef();
		StructBuilder.AddProperty(ChildHandle);
	}
}
