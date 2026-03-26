#include "Games/ActionPracticeGameInstance.h"
#include "GameFramework/GameUserSettings.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeGameInstance, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeGameInstance, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UActionPracticeGameInstance::Init()
{
	Super::Init();

	ApplyDesktopResolution();
}

void UActionPracticeGameInstance::ApplyDesktopResolution()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const FIntPoint DesktopRes = Settings->GetDesktopResolution();

	Settings->SetScreenResolution(DesktopRes);
	Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
	Settings->ApplySettings(false);

	DEBUG_LOG(TEXT("Desktop resolution applied: %d x %d (WindowedFullscreen)"), DesktopRes.X, DesktopRes.Y);
}
