#include "UI/MenuButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogMenuButtonWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogMenuButtonWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UMenuButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	//디자이너 미리보기에도 인스턴스별 LabelText 반영
	if (Label && !LabelText.IsEmpty())
	{
		Label->SetText(LabelText);
	}

	ApplyHoverState(false);
}

void UMenuButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button)
	{
		Button->OnClicked.AddUniqueDynamic(this, &UMenuButtonWidget::HandleButtonClicked);
		Button->OnHovered.AddUniqueDynamic(this, &UMenuButtonWidget::HandleButtonHovered);
		Button->OnUnhovered.AddUniqueDynamic(this, &UMenuButtonWidget::HandleButtonUnhovered);
	}
}

void UMenuButtonWidget::SetLabelText(const FText& InText)
{
	LabelText = InText;

	if (Label)
	{
		Label->SetText(InText);
	}
}

void UMenuButtonWidget::FocusButton()
{
	if (Button)
	{
		Button->SetKeyboardFocus();
	}
}

void UMenuButtonWidget::HandleButtonClicked()
{
	DEBUG_LOG(TEXT("Clicked: %s"), *LabelText.ToString());
	OnMenuButtonClicked.Broadcast();
}

void UMenuButtonWidget::HandleButtonHovered()
{
	ApplyHoverState(true);
}

void UMenuButtonWidget::HandleButtonUnhovered()
{
	ApplyHoverState(false);
}

void UMenuButtonWidget::ApplyHoverState(bool bHovered)
{
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(bHovered ? HoveredTextColor : NormalTextColor));
	}

	if (HoverHighlight)
	{
		HoverHighlight->SetVisibility(bHovered ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
