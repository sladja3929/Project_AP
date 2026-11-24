#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "ActionPracticeGameInstance.generated.h"

UCLASS()
class ACTIONPRACTICE_API UActionPracticeGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	virtual void Init() override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//시연·녹화용 프레임 제한 (화면 캡처가 60fps라 맞춤)
	UPROPERTY(EditDefaultsOnly, Category = "Demo")
	float DemoFrameRateLimit = 60.0f;

	//모니터 해상도와 무관하게 실제 렌더 높이를 이 값 근처로 맞춤 (TSR이 모니터 해상도로 업스케일)
	UPROPERTY(EditDefaultsOnly, Category = "Demo")
	float TargetRenderHeight = 1080.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Demo")
	float MinScreenPercentage = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Demo")
	float MaxScreenPercentage = 75.0f;

#pragma endregion

#pragma region "Protected Functions"

	//모니터 네이티브 해상도를 읽어 WindowedFullscreen으로 적용
	void ApplyDesktopResolution();

	//모니터 해상도에 맞춰 렌더 비율(r.ScreenPercentage) 결정
	void ApplyScreenPercentageForResolution(const FIntPoint& Resolution);

	//포트폴리오 시연용: 스캘러빌리티 품질을 고정 (벤치마크 자동 조정 방지)
	void ApplyScalabilitySettings();

#pragma endregion
};
