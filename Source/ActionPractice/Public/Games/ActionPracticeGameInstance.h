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
#pragma region "Protected Functions"

	//모니터 네이티브 해상도를 읽어 WindowedFullscreen으로 적용
	void ApplyDesktopResolution();

	//포트폴리오 시연용: 스캘러빌리티 품질을 고정 (벤치마크 자동 조정 방지)
	void ApplyScalabilitySettings();

#pragma endregion
};
