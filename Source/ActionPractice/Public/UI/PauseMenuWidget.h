#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UMenuButtonWidget;
class UOverlay;
class UControlsWidget;

DECLARE_MULTICAST_DELEGATE(FOnPauseMenuResumeRequested);

//인게임 ESC 메뉴 위젯
//계속하기·조작법·타이틀로·게임 종료를 제공. 게임은 멈추지 않음 (SetPause 미사용)
UCLASS()
class ACTIONPRACTICE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//계속하기 요청 — 입력 모드·커서 복원을 위해 PlayerController가 바인딩해 메뉴를 닫음
	FOnPauseMenuResumeRequested OnResumeRequested;

#pragma endregion

#pragma region "Public Functions"

	void ShowMenu();
	void HideMenu();
	bool IsMenuVisible() const;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButtonWidget> TitleButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButtonWidget> QuitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMenuButtonWidget> ResumeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMenuButtonWidget> ControlsButton;

	//조작법이 열려 있는 동안 숨기는 메뉴 묶음
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuBox;

	//조작법 위젯을 담을 컨테이너
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> ControlsOverlay;

	//조작법 위젯 클래스 — 타이틀과 같은 WBP_Controls 재사용
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UControlsWidget> ControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UControlsWidget> ControlsWidget = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnResumeButtonClicked();

	UFUNCTION()
	void OnControlsButtonClicked();

	UFUNCTION()
	void OnTitleButtonClicked();

	UFUNCTION()
	void OnQuitButtonClicked();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"

	void HandleControlsClosed();

#pragma endregion
};
