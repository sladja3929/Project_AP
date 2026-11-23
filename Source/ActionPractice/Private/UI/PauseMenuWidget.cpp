#include "UI/PauseMenuWidget.h"
#include "UI/ControlsWidget.h"
#include "UI/MenuButtonWidget.h"
#include "Components/Overlay.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogPauseMenuWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogPauseMenuWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResumeButton)
	{
		ResumeButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnResumeButtonClicked);
	}

	if (ControlsButton)
	{
		ControlsButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnControlsButtonClicked);
	}

	if (TitleButton)
	{
		TitleButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnTitleButtonClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UPauseMenuWidget::OnQuitButtonClicked);
	}

	//조작법 위젯 생성 (초기 Collapsed)
	if (!ControlsWidget)
	{
		ControlsWidget = UControlsWidget::CreateInOverlay(this, ControlsWidgetClass, ControlsOverlay);

		if (ControlsWidget)
		{
			ControlsWidget->OnControlsClosed.AddUObject(this, &UPauseMenuWidget::HandleControlsClosed);
		}
	}
}

void UPauseMenuWidget::ShowMenu()
{
	SetVisibility(ESlateVisibility::Visible);
	DEBUG_LOG(TEXT("PauseMenu shown"));
}

void UPauseMenuWidget::HideMenu()
{
	SetVisibility(ESlateVisibility::Collapsed);

	//조작법이 열린 채로 메뉴가 닫히면 다음에 열 때 조작법부터 보이지 않도록 함께 닫음
	if (ControlsWidget)
	{
		ControlsWidget->CloseControls();
	}

	DEBUG_LOG(TEXT("PauseMenu hidden"));
}

bool UPauseMenuWidget::IsMenuVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UPauseMenuWidget::OnResumeButtonClicked()
{
	DEBUG_LOG(TEXT("Resume button clicked"));
	OnResumeRequested.Broadcast();
}

void UPauseMenuWidget::OnControlsButtonClicked()
{
	if (!ControlsWidget) return;

	if (MenuBox)
	{
		MenuBox->SetVisibility(ESlateVisibility::Hidden);
	}

	ControlsWidget->OpenControls();
	DEBUG_LOG(TEXT("Controls opened from PauseMenu"));
}

void UPauseMenuWidget::HandleControlsClosed()
{
	if (MenuBox)
	{
		MenuBox->SetVisibility(ESlateVisibility::Visible);
	}

	//메뉴가 열려 있을 때만 포커스 복원 (HideMenu에서 닫힌 경우 제외)
	if (IsMenuVisible() && ControlsButton)
	{
		ControlsButton->FocusButton();
	}
}

void UPauseMenuWidget::OnTitleButtonClicked()
{
	DEBUG_LOG(TEXT("Title button clicked — returning to TitleMap"));
	UGameplayStatics::OpenLevel(this, FName(TEXT("TitleMap")));
}

void UPauseMenuWidget::OnQuitButtonClicked()
{
	DEBUG_LOG(TEXT("Quit button clicked — exiting game"));
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
