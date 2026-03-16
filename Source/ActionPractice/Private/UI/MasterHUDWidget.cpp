#include "UI/MasterHUDWidget.h"
#include "UI/PlayerStatsWidget.h"
#include "UI/EquipmentSlotWidget.h"
#include "UI/BossHealthWidget.h"
#include "UI/DeathScreenWidget.h"
#include "UI/InteractionPromptWidget.h"
#include "UI/InteractionResultWidget.h"
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

	//[주의] 모든 자식 위젯의 OverlaySlot은 반드시 HAlign_Fill + VAlign_Fill로 설정할 것.
	//위젯 내부 레이아웃(앵커, 위치, 여백 등)은 WBP 디자이너에서 전적으로 담당한다.
	//C++에서 HAlign_Center/VAlign_Bottom 같은 특정 정렬을 강제하면 WBP에서 설정한 위치가 무시된다.

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

	//BaseLayer — InteractionPrompt (초기 Collapsed)
	if (InteractionPromptWidgetClass && BaseLayer)
	{
		InteractionPromptWidget = CreateWidget<UInteractionPromptWidget>(PC, InteractionPromptWidgetClass);
		if (InteractionPromptWidget)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(BaseLayer->AddChild(InteractionPromptWidget));
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}
			InteractionPromptWidget->HidePrompt();
			DEBUG_LOG(TEXT("InteractionPromptWidget created in BaseLayer (Collapsed)"));
		}
	}

	//BaseLayer — InteractionResult (상호작용 결과 UI 컨테이너)
	if (InteractionResultWidgetClass && BaseLayer)
	{
		InteractionResultWidget = CreateWidget<UInteractionResultWidget>(PC, InteractionResultWidgetClass);
		if (InteractionResultWidget)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(BaseLayer->AddChild(InteractionResultWidget));
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}
			DEBUG_LOG(TEXT("InteractionResultWidget created in BaseLayer"));
		}
	}
}

void UMasterHUDWidget::ShowItemAcquisition(const UBaseItemDataAsset* InItemDA, int32 InCount)
{
	if (InteractionResultWidget)
	{
		InteractionResultWidget->AddItemAcquisitionNotification(InItemDA, InCount);
	}
}

void UMasterHUDWidget::ShowInteractionPrompt(const FText& InPromptText)
{
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->ShowPrompt(InPromptText);
	}
}

void UMasterHUDWidget::HideInteractionPrompt()
{
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->HidePrompt();
	}
}

void UMasterHUDWidget::SetInteractionPromptDimmed(bool bDimmed)
{
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->SetDimmed(bDimmed);
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
