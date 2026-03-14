#include "UI/MasterHUDWidget.h"
#include "UI/PlayerStatsWidget.h"
#include "UI/EquipmentSlotWidget.h"
#include "UI/BossHealthWidget.h"
#include "UI/DeathScreenWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogMasterHUDWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogMasterHUDWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UMasterHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	//자식 위젯 생성은 AddToViewport 이후 컨트롤러에서 명시적으로 호출
}

void UMasterHUDWidget::CreateChildWidgets()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	//BaseLayer — PlayerStats
	if (PlayerStatsWidgetClass && BaseLayer)
	{
		PlayerStatsWidget = CreateWidget<UPlayerStatsWidget>(PC, PlayerStatsWidgetClass);
		if (PlayerStatsWidget)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(BaseLayer->AddChild(PlayerStatsWidget));
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}
			DEBUG_LOG(TEXT("PlayerStatsWidget created in BaseLayer"));
		}
	}

	//BaseLayer — EquipmentSlot
	if (EquipmentSlotWidgetClass && BaseLayer)
	{
		EquipmentSlotWidget = CreateWidget<UEquipmentSlotWidget>(PC, EquipmentSlotWidgetClass);
		if (EquipmentSlotWidget)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(BaseLayer->AddChild(EquipmentSlotWidget));
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}
			DEBUG_LOG(TEXT("EquipmentSlotWidget created in BaseLayer"));
		}
	}

	//WorldEventLayer — BossHealth (초기 Collapsed)
	if (BossHealthWidgetClass && WorldEventLayer)
	{
		BossHealthWidget = CreateWidget<UBossHealthWidget>(PC, BossHealthWidgetClass);
		if (BossHealthWidget)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(WorldEventLayer->AddChild(BossHealthWidget));
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}
			WorldEventLayer->SetVisibility(ESlateVisibility::Collapsed);
			DEBUG_LOG(TEXT("BossHealthWidget created in WorldEventLayer (Collapsed)"));
		}
	}

	//ModalLayer — DeathScreen (초기 숨김)
	if (DeathScreenWidgetClass && ModalLayer)
	{
		DeathScreenWidget = CreateWidget<UDeathScreenWidget>(PC, DeathScreenWidgetClass);
		if (DeathScreenWidget)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(ModalLayer->AddChild(DeathScreenWidget));
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}
			DeathScreenWidget->SetDeathScreenVisibility(false);
			DEBUG_LOG(TEXT("DeathScreenWidget created in ModalLayer (Hidden)"));
		}
	}
}

void UMasterHUDWidget::BindPlayerData(UActionPracticeAttributeSet* InAttributeSet, UWeaponManagerComponent* InWeaponManager, UItemManagerComponent* InItemManager)
{
	if (PlayerStatsWidget && InAttributeSet)
	{
		PlayerStatsWidget->SetAttributeSet(InAttributeSet);
		DEBUG_LOG(TEXT("PlayerStatsWidget AttributeSet bound"));
	}

	if (EquipmentSlotWidget && InWeaponManager && InItemManager)
	{
		EquipmentSlotWidget->SetDataSources(InWeaponManager, InItemManager);
		DEBUG_LOG(TEXT("EquipmentSlotWidget data sources bound"));
	}
}

void UMasterHUDWidget::ShowBossHealth(UBossAttributeSet* InBossAttributeSet, const FName& InBossName)
{
	if (!BossHealthWidget) return;

	if (InBossAttributeSet)
	{
		BossHealthWidget->SetBossAttributeSet(InBossAttributeSet);
	}
	BossHealthWidget->SetBossName(InBossName);
	WorldEventLayer->SetVisibility(ESlateVisibility::HitTestInvisible);

	DEBUG_LOG(TEXT("BossHealth shown: %s"), *InBossName.ToString());
}

void UMasterHUDWidget::HideBossHealth()
{
	if (!WorldEventLayer) return;
	WorldEventLayer->SetVisibility(ESlateVisibility::Collapsed);

	DEBUG_LOG(TEXT("BossHealth hidden"));
}

void UMasterHUDWidget::HandleDeadStateStart()
{
	if (DeathScreenWidget)
	{
		DeathScreenWidget->HandleDeadStateStart();
	}
}

void UMasterHUDWidget::HandleDeadStateFinish()
{
	if (DeathScreenWidget)
	{
		DeathScreenWidget->HandleDeadStateFinish();
	}
}

void UMasterHUDWidget::SetDeathScreenVisibility(bool bShow)
{
	if (DeathScreenWidget)
	{
		DeathScreenWidget->SetDeathScreenVisibility(bShow);
	}
}
