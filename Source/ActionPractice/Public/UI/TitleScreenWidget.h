#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleScreenWidget.generated.h"

class UMenuButtonWidget;
class UOverlay;
class UControlsWidget;

UCLASS()
class ACTIONPRACTICE_API UTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"
#pragma endregion

protected:
#pragma region "Protected Variables"

	//WBP에서 BindWidget으로 연결
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButtonWidget> ControlsButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButtonWidget> TestMapButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButtonWidget> MainMapButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMenuButtonWidget> QuitButton;

	//등장 페이드인 대상 — 로고 먼저, 메뉴는 MenuFadeInDelay 뒤
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LogoBox;

	//조작법이 열려 있는 동안 숨기는 메뉴 버튼 묶음
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuBox;

	UPROPERTY(EditDefaultsOnly, Category = "Fade In", meta = (ClampMin = "0.0"))
	float LogoFadeInDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Fade In", meta = (ClampMin = "0.0"))
	float MenuFadeInDelay = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Fade In", meta = (ClampMin = "0.0"))
	float MenuFadeInDuration = 1.0f;

	//버튼별 이동할 맵 — WBP에서 지정
	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TSoftObjectPtr<UWorld> TestMapLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TSoftObjectPtr<UWorld> MainMapLevel;

	//조작법 위젯 클래스 — EditDefaultsOnly로 WBP에서 지정
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UControlsWidget> ControlsWidgetClass;

	//생성된 조작법 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UControlsWidget> ControlsWidget = nullptr;

	//조작법 위젯을 담을 컨테이너 — WBP에서 BindWidget으로 연결
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> ControlsOverlay;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void OnControlsButtonClicked();

	UFUNCTION()
	void OnTestMapButtonClicked();

	UFUNCTION()
	void OnMainMapButtonClicked();

	UFUNCTION()
	void OnQuitButtonClicked();

#pragma endregion

private:
#pragma region "Private Variables"

	float FadeInElapsed = 0.f;
	bool bFadeInFinished = true;

#pragma endregion

#pragma region "Private Functions"

	void HandleControlsClosed();
	void OpenLevelSafe(const TSoftObjectPtr<UWorld>& Level, const TCHAR* DebugName);

	//Delay 이후 Duration 동안 0→1 진행률
	static float GetFadeAlpha(float Elapsed, float Delay, float Duration);

#pragma endregion
};
