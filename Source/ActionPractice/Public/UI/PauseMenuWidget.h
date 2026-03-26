#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;

//인게임 ESC 메뉴 위젯
//게임 종료 버튼을 제공
UCLASS()
class ACTIONPRACTICE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"

	void ShowMenu();
	void HideMenu();
	bool IsMenuVisible() const;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuitButton;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnQuitButtonClicked();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
