#include "Customization/EnemyComboAttackUnitCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Items/AttackData.h"

TSharedRef<IPropertyTypeCustomization> FEnemyComboAttackUnitCustomization::MakeInstance()
{
	return MakeShareable(new FEnemyComboAttackUnitCustomization());
}

void FEnemyComboAttackUnitCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	//ComboData > AttackMontage 경로
	TSharedPtr<IPropertyHandle> ComboDataHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyComboAttackUnit, ComboData));
	TSharedPtr<IPropertyHandle> MontageHandle;
	if (ComboDataHandle.IsValid())
	{
		MontageHandle = ComboDataHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FComboAttackUnit, AttackMontage));
	}

	TSharedPtr<IPropertyHandle> RotateHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyComboAttackUnit, RotateTime));
	TSharedPtr<IPropertyHandle> DistanceHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyComboAttackUnit, MaxTargetDistance));
	TSharedPtr<IPropertyHandle> AngleHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnemyComboAttackUnit, MaxTargetAngle));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(500.0f)
	[
		SNew(SHorizontalBox)

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

				//SoftObjectPath에서 에셋 이름만 추출
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

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			.Text_Lambda([RotateHandle, DistanceHandle, AngleHandle]() -> FText
			{
				float Rot = 0.1f, Dist = 150.0f, Ang = 60.0f;
				if (RotateHandle.IsValid()) RotateHandle->GetValue(Rot);
				if (DistanceHandle.IsValid()) DistanceHandle->GetValue(Dist);
				if (AngleHandle.IsValid()) AngleHandle->GetValue(Ang);

				return FText::FromString(FString::Printf(
					TEXT("| Rot:%.2f  Dist:%.0f  Ang:%.0f"), Rot, Dist, Ang));
			})
		]
	];
}

void FEnemyComboAttackUnitCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(i).ToSharedRef();
		StructBuilder.AddProperty(ChildHandle);
	}
}
