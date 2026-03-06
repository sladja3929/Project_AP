#include "UI/EquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Characters/WeaponManagerComponent.h"
#include "Characters/ItemManagerComponent.h"
#include "Items/BaseItemDataAsset.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/UsableItemDataAsset.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEquipmentSlotWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEquipmentSlotWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEquipmentSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//초기 상태: 빈 슬롯
	if (LeftWeaponIcon) SetSlotIcon(LeftWeaponIcon, nullptr);
	if (RightWeaponIcon) SetSlotIcon(RightWeaponIcon, nullptr);
	if (UsableItemIcon) SetSlotIcon(UsableItemIcon, nullptr);
	if (UsableItemCountText) UsableItemCountText->SetText(FText::GetEmpty());
}

void UEquipmentSlotWidget::NativeDestruct()
{
	//델리게이트 해제
	if (WeaponManager)
	{
		WeaponManager->OnWeaponChanged.RemoveDynamic(this, &UEquipmentSlotWidget::RefreshWeaponSlots);
	}
	if (ItemManager)
	{
		ItemManager->OnEquippedItemChanged.RemoveDynamic(this, &UEquipmentSlotWidget::RefreshItemSlot);
	}

	Super::NativeDestruct();
}

void UEquipmentSlotWidget::SetDataSources(UWeaponManagerComponent* InWeaponManager, UItemManagerComponent* InItemManager)
{
	WeaponManager = InWeaponManager;
	ItemManager = InItemManager;

	if (WeaponManager)
	{
		WeaponManager->OnWeaponChanged.AddDynamic(this, &UEquipmentSlotWidget::RefreshWeaponSlots);
		RefreshWeaponSlots();
	}

	if (ItemManager)
	{
		ItemManager->OnEquippedItemChanged.AddDynamic(this, &UEquipmentSlotWidget::RefreshItemSlot);
		RefreshItemSlot();
	}
}

void UEquipmentSlotWidget::RefreshWeaponSlots()
{
	if (!WeaponManager) return;

	//좌무기
	AWeapon* LeftWeapon = WeaponManager->GetLeftWeapon();
	const UBaseItemDataAsset* LeftDA = LeftWeapon ? LeftWeapon->GetWeaponData() : nullptr;
	SetSlotIcon(LeftWeaponIcon, LeftDA);

	//우무기
	AWeapon* RightWeapon = WeaponManager->GetRightWeapon();
	const UBaseItemDataAsset* RightDA = RightWeapon ? RightWeapon->GetWeaponData() : nullptr;
	SetSlotIcon(RightWeaponIcon, RightDA);
}

void UEquipmentSlotWidget::RefreshItemSlot()
{
	if (!ItemManager) return;

	const FUsableItemSlot& CurrentSlot = ItemManager->GetEquippedSlot();

	//아이콘
	SetSlotIcon(UsableItemIcon, CurrentSlot.ItemDA);

	//수량 (무제한 아이템이면 비표시, 그 외에는 숫자)
	if (UsableItemCountText)
	{
		if (CurrentSlot.IsValid() && CurrentSlot.ItemDA && !CurrentSlot.ItemDA->IsUnlimited())
		{
			UsableItemCountText->SetText(FText::AsNumber(CurrentSlot.CurrentCount));
		}
		else
		{
			UsableItemCountText->SetText(FText::GetEmpty());
		}
	}
}

void UEquipmentSlotWidget::SetSlotIcon(UImage* TargetImage, const UBaseItemDataAsset* ItemDA)
{
	if (!TargetImage) return;

	UTexture2D* IconTexture = nullptr;

	if (ItemDA && !ItemDA->Icon.IsNull())
	{
		//TSoftObjectPtr 동기 로드
		IconTexture = ItemDA->Icon.LoadSynchronous();
	}

	if (IconTexture)
	{
		TargetImage->SetBrushFromTexture(IconTexture);
		TargetImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else if (EmptySlotTexture)
	{
		TargetImage->SetBrushFromTexture(EmptySlotTexture);
		TargetImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		TargetImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}
