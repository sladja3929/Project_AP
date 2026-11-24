#include "Games/ActionPracticeGameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"

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

	//에디터 PIE는 에디터 뷰포트 기준이라 제외
	if (!GIsEditor)
	{
		ApplyScreenPercentageForResolution(DesktopRes);
	}
}

void UActionPracticeGameInstance::ApplyScreenPercentageForResolution(const FIntPoint& Resolution)
{
	if (Resolution.Y <= 0)
	{
		return;
	}

	//예: 1080p·1440p → 75%, 4K → 50%
	const float ScreenPercentage = FMath::Clamp(TargetRenderHeight / static_cast<float>(Resolution.Y) * 100.0f, MinScreenPercentage, MaxScreenPercentage);

	//ini([SystemSettings])보다 높은 우선순위로 설정
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		CVar->Set(ScreenPercentage, ECVF_SetByCode);
	}

	DEBUG_LOG(TEXT("Screen percentage %.1f%% for %d x %d (render %d x %d)"), ScreenPercentage, Resolution.X, Resolution.Y,
		FMath::RoundToInt(Resolution.X * ScreenPercentage / 100.0f), FMath::RoundToInt(Resolution.Y * ScreenPercentage / 100.0f));
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

	//시연·녹화용 60fps 고정 (엔진이 GameInstance 생성 직전에 사용자 설정 프레임 제한을 적용하므로 이후 시점에서 덮어씀)
	//에디터 PIE에서는 에디터 프레임까지 묶이지 않도록 제외
	if (!GIsEditor)
	{
		Settings->SetFrameRateLimit(DemoFrameRateLimit);
	}

	Settings->ApplySettings(false);

	DEBUG_LOG(TEXT("Scalability settings locked for demo build (FrameRateLimit %.0f)"), Settings->GetFrameRateLimit());
}
