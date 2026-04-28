#include "UI/PauseMenuWidget.h"
#include "Components/Button.h"
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

	if (TitleButton)
	{
		TitleButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnTitleButtonClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuitButtonClicked);
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
	DEBUG_LOG(TEXT("PauseMenu hidden"));
}

bool UPauseMenuWidget::IsMenuVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
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
