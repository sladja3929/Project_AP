#include "Customization/ComboAttackUnitCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
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
	//부모 구조체의 AttackType 핸들 탐색
	TSharedPtr<IPropertyHandle> AttackTypeHandle = FindParentAttackTypeHandle(StructPropertyHandle);

	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(i).ToSharedRef();

		//SubAttackMontage 필드: AttackType이 Charge일 때만 표시
		if (ChildHandle->GetProperty() && ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FComboAttackUnit, SubAttackMontage))
		{
			if (AttackTypeHandle.IsValid())
			{
				IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle);
				Row.Visibility(TAttribute<EVisibility>::CreateLambda([AttackTypeHandle]()
				{
					uint8 Val = 0;
					if (AttackTypeHandle.IsValid())
					{
						AttackTypeHandle->GetValue(Val);
					}
					return static_cast<EComboAttackType>(Val) == EComboAttackType::Charge
						? EVisibility::Visible : EVisibility::Collapsed;
				}));
				continue;
			}
			//AttackType 핸들을 못 찾으면 기본 표시 (fallback)
		}

		StructBuilder.AddProperty(ChildHandle);
	}
}

TSharedPtr<IPropertyHandle> FComboAttackUnitCustomization::FindParentAttackTypeHandle(TSharedRef<IPropertyHandle> StructPropertyHandle, int32 MaxDepth)
{
	//PropertyHandle 체인을 역추적하며 "AttackType" 자식이 있는 부모를 찾는다
	//
	//WeaponDA 경로 (2홉):
	//  FComboAttackUnit[N] → ComboSequence(TArray) → FTaggedAttackData → AttackType
	//
	//EnemyDA 경로 (3홉):
	//  FComboAttackUnit(ComboData) → FEnemyComboAttackUnit[N] → ComboSequence(TArray) → FEnemyTaggedAttackData → AttackType

	TSharedPtr<IPropertyHandle> Current = StructPropertyHandle->GetParentHandle();

	for (int32 Depth = 0; Depth < MaxDepth && Current.IsValid(); ++Depth)
	{
		TSharedPtr<IPropertyHandle> AttackTypeChild = Current->GetChildHandle(TEXT("AttackType"));
		if (AttackTypeChild.IsValid() && AttackTypeChild->IsValidHandle())
		{
			return AttackTypeChild;
		}
		Current = Current->GetParentHandle();
	}

	return nullptr;
}
