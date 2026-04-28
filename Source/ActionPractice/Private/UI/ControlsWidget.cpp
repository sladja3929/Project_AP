#include "UI/ControlsWidget.h"
#include "Components/Button.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogControlsWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogControlsWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UControlsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UControlsWidget::OnCloseButtonClicked);
	}
}

void UControlsWidget::OnCloseButtonClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
	DEBUG_LOG(TEXT("ControlsWidget closed"));
}
