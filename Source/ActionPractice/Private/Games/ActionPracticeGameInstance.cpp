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
	ApplyScalabilitySettings();
}

void UActionPracticeGameInstance::ApplyDesktopResolution()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const FIntPoint DesktopRes = Settings->GetDesktopResolution();

	Settings->SetScreenResolution(DesktopRes);
	Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
	Settings->ApplyResolutionSettings(false);

	DEBUG_LOG(TEXT("Desktop resolution applied: %d x %d (WindowedFullscreen)"), DesktopRes.X, DesktopRes.Y);
}

void UActionPracticeGameInstance::ApplyScalabilitySettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	//DefaultGameUserSettings.ini의 초기값을 매 실행마다 강제 재적용
	//(벤치마크 자동 조정이나 사용자 저장값이 우선 적용되는 것을 방지)
	Settings->SetViewDistanceQuality(2);
	Settings->SetAntiAliasingQuality(2);
	Settings->SetShadowQuality(1);
	Settings->SetGlobalIlluminationQuality(1);
	Settings->SetReflectionQuality(1);
	Settings->SetPostProcessingQuality(1);
	Settings->SetTextureQuality(2);
	Settings->SetVisualEffectQuality(1);
	Settings->SetFoliageQuality(1);
	Settings->SetShadingQuality(2);

	Settings->ApplySettings(false);

	DEBUG_LOG(TEXT("Scalability settings locked for demo build"));
}
