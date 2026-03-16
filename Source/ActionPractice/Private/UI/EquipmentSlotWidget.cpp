#include "UI/EquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Characters/WeaponManagerComponent.h"
#include "Characters/ItemManagerComponent.h"
#include "Items/BaseItemDataAsset.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/UsableItemDataAsset.h"

#define ENABLE_DEBUG_LOG 0

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
	if (NextUsableItemIcon1) SetSlotIcon(NextUsableItemIcon1, nullptr);
	if (NextUsableItemIcon2) SetSlotIcon(NextUsableItemIcon2, nullptr);
}

void UEquipmentSlotWidget::NativeDestruct()
{
	//델리게이트 해제
	if (WeaponManager)
	{
		WeaponManager->OnWeaponChanged.RemoveDynamic(this, &UEquipmentSlotWidget::RefreshWeaponSlot);
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
		WeaponManager->OnWeaponChanged.AddDynamic(this, &UEquipmentSlotWidget::RefreshWeaponSlot);
		RefreshWeaponSlot(true);
		RefreshWeaponSlot(false);
	}

	if (ItemManager)
	{
		ItemManager->OnEquippedItemChanged.AddDynamic(this, &UEquipmentSlotWidget::RefreshItemSlot);
		RefreshItemSlot();
	}
}

void UEquipmentSlotWidget::RefreshWeaponSlot(bool bIsLeftHand)
{
	if (!WeaponManager) return;
	DEBUG_LOG(TEXT("RefreshWeaponSlot: WeaponChanged Broadcasted, IsLeft=%d"), bIsLeftHand);
	
	if (bIsLeftHand)
	{
		AWeapon* ChangedWeapon = WeaponManager->GetLeftWeapon();
		const UBaseItemDataAsset* LeftDA = ChangedWeapon ? ChangedWeapon->GetWeaponData() : nullptr;
		SetSlotIcon(LeftWeaponIcon, LeftDA);
	}
	
	else
	{
		AWeapon* ChangedWeapon = WeaponManager->GetRightWeapon();
		const UBaseItemDataAsset* RightDA = ChangedWeapon ? ChangedWeapon->GetWeaponData() : nullptr;
		SetSlotIcon(RightWeaponIcon, RightDA);
	}	
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
		if (CurrentSlot.IsValid() && CurrentSlot.ItemDA && CurrentSlot.ItemDA->HasCount())
		{
			UsableItemCountText->SetText(FText::AsNumber(CurrentSlot.CurrentCount));
		}
		else
		{
			UsableItemCountText->SetText(FText::GetEmpty());
		}
	}

	//프리뷰 슬롯 갱신 (슬롯이 2개 이하이면 프리뷰 의미 없음)
	if (NextUsableItemIcon1)
	{
		if (ItemManager->GetSlotCount() > 1)
		{
			const FUsableItemSlot& NextSlot1 = ItemManager->GetSlotAtOffset(1);
			SetSlotIcon(NextUsableItemIcon1, NextSlot1.ItemDA);
		}
		else
		{
			SetSlotIcon(NextUsableItemIcon1, nullptr);
		}
	}

	if (NextUsableItemIcon2)
	{
		if (ItemManager->GetSlotCount() > 2)
		{
			const FUsableItemSlot& NextSlot2 = ItemManager->GetSlotAtOffset(2);
			SetSlotIcon(NextUsableItemIcon2, NextSlot2.ItemDA);
		}
		else
		{
			SetSlotIcon(NextUsableItemIcon2, nullptr);
		}
	}
}

void UEquipmentSlotWidget::SetSlotIcon(UImage* TargetImage, const UBaseItemDataAsset* ItemDA)
{
	if (!TargetImage)
	{
		DEBUG_LOG(TEXT("SlotIcon: No TargetImage"));
		return;
	}

	UTexture2D* IconTexture = nullptr;

	if (ItemDA && !ItemDA->Icon.IsNull())
	{
		//TSoftObjectPtr 동기 로드
		IconTexture = ItemDA->Icon.LoadSynchronous();
	}

	if (IconTexture)
	{
		DEBUG_LOG(TEXT("SlotIcon: Texture=%s (ptr=%p)"), *GetNameSafe(IconTexture), IconTexture);
		TargetImage->SetBrushFromTexture(IconTexture);
		TargetImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DEBUG_LOG(TEXT("SlotIcon: Icon Changed"));
	}

	//할당된 아이콘이 없으면 기본(비었음) 아이콘 사용
	else if (EmptySlotTexture)
	{
		TargetImage->SetBrushFromTexture(EmptySlotTexture);
		TargetImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DEBUG_LOG(TEXT("SlotIcon: No Icon, EmptySlotTexture"));
	}
	
	else
	{
		TargetImage->SetVisibility(ESlateVisibility::Collapsed);
		DEBUG_LOG(TEXT("SlotIcon: No Icon, No EmptySlotTexture"));
	}
}
