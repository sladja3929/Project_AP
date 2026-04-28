#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ControlsWidget.generated.h"

class UButton;

//조작법 표시 위젯
//키바인딩 목록을 표시하고 닫기 버튼을 제공
UCLASS()
class ACTIONPRACTICE_API UControlsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"
#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnCloseButtonClicked();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
