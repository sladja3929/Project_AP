#include "UI/EquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Characters/WeaponManagerComponent.h"
#include "Characters/ItemManagerComponent.h"
#include "Items/BaseItemDataAsset.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/UsableItemDataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"

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
	if (UsableItemNameText) UsableItemNameText->SetText(FText::GetEmpty());
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

	//진행 중인 모든 아이콘 비동기 로드 취소 (위젯 파괴 후 콜백이 파괴된 이미지를 건드리지 않도록)
	for (TPair<UImage*, FSlotIconRequest>& Pair : PendingIconRequests)
	{
		if (Pair.Value.Handle.IsValid())
		{
			Pair.Value.Handle->CancelHandle();
		}
	}
	PendingIconRequests.Empty();

	Super::NativeDestruct();
}

void UEquipmentSlotWidget::SetDataSources(UWeaponManagerComponent* InWeaponManager, UItemManagerComponent* InItemManager)
{
	//재호출 시 이전 바인딩 먼저 제거 — AddDynamic 중복 바인딩 크래시 방지
	if (WeaponManager)
	{
		WeaponManager->OnWeaponChanged.RemoveDynamic(this, &UEquipmentSlotWidget::RefreshWeaponSlot);
	}
	if (ItemManager)
	{
		ItemManager->OnEquippedItemChanged.RemoveDynamic(this, &UEquipmentSlotWidget::RefreshItemSlot);
	}

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

	//아이템 이름 갱신
	if (UsableItemNameText)
	{
		if (CurrentSlot.IsValid() && CurrentSlot.ItemDA)
		{
			UsableItemNameText->SetText(CurrentSlot.ItemDA->DisplayName);
		}
		else
		{
			UsableItemNameText->SetText(FText::GetEmpty());
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

	//이 이미지에 진행 중이던 이전 아이콘 로드 취소 (슬롯 재사용 시 오래된 콜백이 새 아이콘을 덮어쓰지 않도록)
	CancelPendingIconLoad(TargetImage);

	//아이콘이 지정되지 않은 슬롯 — 빈 텍스처/숨김 즉시 반영
	if (!ItemDA || ItemDA->Icon.IsNull())
	{
		ApplyEmptySlotIcon(TargetImage);
		return;
	}

	const TSoftObjectPtr<UTexture2D> SoftIcon = ItemDA->Icon;

	//이미 로드된 경우(이전 로드/프리로드로 메모리에 존재) 즉시 반영
	if (UTexture2D* Loaded = SoftIcon.Get())
	{
		DEBUG_LOG(TEXT("SlotIcon: Texture=%s (already loaded)"), *GetNameSafe(Loaded));
		TargetImage->SetBrushFromTexture(Loaded);
		TargetImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	//AssetManager 미초기화 등 비동기 불가 시 동기 폴백
	if (!UAssetManager::IsInitialized())
	{
		if (UTexture2D* Sync = SoftIcon.LoadSynchronous())
		{
			TargetImage->SetBrushFromTexture(Sync);
			TargetImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			ApplyEmptySlotIcon(TargetImage);
		}
		return;
	}

	//로드 중에는 빈 슬롯 텍스처 표시, 완료 콜백에서 실제 아이콘 반영
	ApplyEmptySlotIcon(TargetImage);

	const FSoftObjectPath IconPath = SoftIcon.ToSoftObjectPath();
	TWeakObjectPtr<UEquipmentSlotWidget> WeakThis(this);
	TWeakObjectPtr<UImage> WeakImage(TargetImage);

	FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		IconPath,
		FStreamableDelegate::CreateLambda([WeakThis, WeakImage, SoftIcon, IconPath]()
		{
			UEquipmentSlotWidget* StrongThis = WeakThis.Get();
			UImage* Image = WeakImage.Get();

			//위젯/이미지가 파괴됐으면 조용히 반환
			if (!StrongThis || !Image)
			{
				return;
			}

			//세대 검증: 이 콜백의 요청 경로가 이 이미지의 최신 요청과 일치할 때만 반영 (슬롯 재사용/풀링 대비)
			const FSlotIconRequest* Latest = StrongThis->PendingIconRequests.Find(Image);
			if (!Latest || Latest->Path != IconPath)
			{
				return;
			}

			if (UTexture2D* Texture = SoftIcon.Get())
			{
				Image->SetBrushFromTexture(Texture);
				Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}

			//반영 완료 — 브러시(UImage UPROPERTY)가 텍스처 참조를 유지하므로 핸들 해제 안전
			StrongThis->PendingIconRequests.Remove(Image);
		}),
		FStreamableManager::DefaultAsyncLoadPriority);

	FSlotIconRequest Request;
	Request.Handle = Handle;
	Request.Path = IconPath;
	PendingIconRequests.Add(TargetImage, MoveTemp(Request));
}

void UEquipmentSlotWidget::CancelPendingIconLoad(UImage* TargetImage)
{
	if (FSlotIconRequest* Existing = PendingIconRequests.Find(TargetImage))
	{
		if (Existing->Handle.IsValid())
		{
			Existing->Handle->CancelHandle();
		}
		PendingIconRequests.Remove(TargetImage);
	}
}

void UEquipmentSlotWidget::ApplyEmptySlotIcon(UImage* TargetImage)
{
	if (!TargetImage)
	{
		return;
	}

	//할당된 아이콘이 없으면 기본(비었음) 아이콘 사용
	if (EmptySlotTexture)
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
