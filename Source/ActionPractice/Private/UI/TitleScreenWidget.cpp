#include "UI/TitleScreenWidget.h"
#include "UI/ControlsWidget.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Kismet/GameplayStatics.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTitleScreenWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogTitleScreenWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UTitleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ControlsButton)
	{
		ControlsButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnControlsButtonClicked);
	}

	if (TestMapButton)
	{
		TestMapButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnTestMapButtonClicked);
	}

	if (MainMapButton)
	{
		MainMapButton->OnClicked.AddDynamic(this, &UTitleScreenWidget::OnMainMapButtonClicked);
	}

	//조작법 위젯 생성 (초기 Collapsed)
	if (ControlsWidgetClass && ControlsOverlay)
	{
		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			ControlsWidget = CreateWidget<UControlsWidget>(PC, ControlsWidgetClass);
			if (ControlsWidget)
			{
				UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(ControlsOverlay->AddChild(ControlsWidget));
				if (OverlaySlot)
				{
					OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
					OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
				}
				ControlsWidget->SetVisibility(ESlateVisibility::Collapsed);
				DEBUG_LOG(TEXT("ControlsWidget created (Collapsed)"));
			}
		}
	}
}

void UTitleScreenWidget::OnControlsButtonClicked()
{
	if (!ControlsWidget) return;

	const bool bIsVisible = ControlsWidget->GetVisibility() == ESlateVisibility::Visible;
	ControlsWidget->SetVisibility(bIsVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	DEBUG_LOG(TEXT("Controls toggled: %s"), bIsVisible ? TEXT("Hidden") : TEXT("Shown"));
}

void UTitleScreenWidget::OnTestMapButtonClicked()
{
	if (TestMapLevel.IsNull())
	{
		DEBUG_LOG(TEXT("OnTestMapButtonClicked: TestMapLevel not set"));
		return;
	}

	const FString MapPath = TestMapLevel.GetLongPackageName();
	DEBUG_LOG(TEXT("Opening TestMap: %s"), *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}

void UTitleScreenWidget::OnMainMapButtonClicked()
{
	if (MainMapLevel.IsNull())
	{
		DEBUG_LOG(TEXT("OnMainMapButtonClicked: MainMapLevel not set"));
		return;
	}

	const FString MapPath = MainMapLevel.GetLongPackageName();
	DEBUG_LOG(TEXT("Opening MainMap: %s"), *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}
