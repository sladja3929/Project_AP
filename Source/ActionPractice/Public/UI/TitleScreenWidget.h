#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleScreenWidget.generated.h"

class UButton;
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
	TObjectPtr<UButton> ControlsButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> TestMapButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MainMapButton;

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

	UFUNCTION()
	void OnControlsButtonClicked();

	UFUNCTION()
	void OnTestMapButtonClicked();

	UFUNCTION()
	void OnMainMapButtonClicked();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
