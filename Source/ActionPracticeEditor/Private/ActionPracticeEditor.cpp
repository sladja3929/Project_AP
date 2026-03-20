#include "ActionPracticeEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

//커스터마이제이션 헤더
#include "Customization/EnemyTaggedAttackDataCustomization.h"
#include "Customization/EnemyComboAttackUnitCustomization.h"
#include "Customization/TaggedAttackDataCustomization.h"
#include "Customization/ComboAttackUnitCustomization.h"

//게임 모듈 구조체 헤더
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Items/WeaponDataAsset.h"
#include "Items/AttackData.h"

void FActionPracticeEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	//Enemy DA 커스터마이제이션
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FEnemyTaggedAttackData::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEnemyTaggedAttackDataCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout(
		FEnemyComboAttackUnit::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEnemyComboAttackUnitCustomization::MakeInstance)
	);

	//Weapon DA 커스터마이제이션
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FTaggedAttackData::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTaggedAttackDataCustomization::MakeInstance)
	);

	//공용 콤보 단위 커스터마이제이션
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FComboAttackUnit::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FComboAttackUnitCustomization::MakeInstance)
	);

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FActionPracticeEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomPropertyTypeLayout(FEnemyTaggedAttackData::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FEnemyComboAttackUnit::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FTaggedAttackData::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FComboAttackUnit::StaticStruct()->GetFName());
	}
}

IMPLEMENT_MODULE(FActionPracticeEditorModule, ActionPracticeEditor)
