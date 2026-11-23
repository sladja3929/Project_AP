#include "UI/ControlsWidget.h"
#include "UI/MenuButtonWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InputCoreTypes.h"

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
		CloseButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UControlsWidget::OnCloseButtonClicked);
	}
}

UControlsWidget* UControlsWidget::CreateInOverlay(UUserWidget* OwnerWidget, TSubclassOf<UControlsWidget> WidgetClass, UOverlay* Overlay)
{
	if (!OwnerWidget || !WidgetClass || !Overlay) return nullptr;

	UControlsWidget* NewWidget = CreateWidget<UControlsWidget>(OwnerWidget, WidgetClass);
	if (!NewWidget) return nullptr;

	UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Overlay->AddChild(NewWidget));

	if (OverlaySlot)
	{
		OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
	}

	NewWidget->SetVisibility(ESlateVisibility::Collapsed);
	DEBUG_LOG(TEXT("ControlsWidget created in %s (Collapsed)"), *GetNameSafe(OwnerWidget));
	return NewWidget;
}

void UControlsWidget::OpenControls()
{
	SetVisibility(ESlateVisibility::Visible);

	//ESC가 이 위젯까지 버블링되도록 내부 버튼에 포커스
	if (CloseButton)
	{
		CloseButton->FocusButton();
	}

	DEBUG_LOG(TEXT("ControlsWidget opened"));
}

void UControlsWidget::CloseControls()
{
	if (!IsOpen()) return;

	SetVisibility(ESlateVisibility::Collapsed);
	OnControlsClosed.Broadcast();
	DEBUG_LOG(TEXT("ControlsWidget closed"));
}

bool UControlsWidget::IsOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

FReply UControlsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	//ESC·게임패드 B로 닫기. 처리한 키는 게임 입력(IA_Pause)으로 넘어가지 않음
	const FKey Key = InKeyEvent.GetKey();

	if (IsOpen() && (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right))
	{
		CloseControls();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UControlsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsOpen())
	{
		if (CloseButton)
		{
			CloseButton->FocusButton();
		}

		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UControlsWidget::OnCloseButtonClicked()
{
	CloseControls();
}
