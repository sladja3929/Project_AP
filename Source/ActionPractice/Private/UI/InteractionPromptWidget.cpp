#include "UI/InteractionPromptWidget.h"
#include "Components/TextBlock.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogInteractionPromptWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogInteractionPromptWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UInteractionPromptWidget::ShowPrompt(const FText& InPromptText)
{
	if (PromptTextBlock)
	{
		PromptTextBlock->SetText(InPromptText);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DEBUG_LOG(TEXT("ShowPrompt: %s"), *InPromptText.ToString());
}

void UInteractionPromptWidget::HidePrompt()
{
	SetVisibility(ESlateVisibility::Collapsed);
	DEBUG_LOG(TEXT("HidePrompt"));
}

void UInteractionPromptWidget::SetDimmed(bool bDimmed)
{
	SetRenderOpacity(bDimmed ? DimmedOpacity : 1.0f);
	DEBUG_LOG(TEXT("SetDimmed: %s (Opacity=%.2f)"), bDimmed ? TEXT("true") : TEXT("false"), GetRenderOpacity());
}
