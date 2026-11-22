#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ControlsWidget.generated.h"

class UMenuButtonWidget;
class UOverlay;

DECLARE_MULTICAST_DELEGATE(FOnControlsClosed);

//조작법 표시 위젯
//타이틀·일시정지 메뉴가 각자의 오버레이에 생성해 재사용. 닫기 버튼 또는 ESC로 닫힘
UCLASS()
class ACTIONPRACTICE_API UControlsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//닫힘 알림 — 소유 메뉴가 숨겨 둔 메뉴 버튼을 복원
	FOnControlsClosed OnControlsClosed;

#pragma endregion

#pragma region "Public Functions"

	//소유 위젯의 오버레이를 꽉 채우도록 생성·추가 (초기 Collapsed)
	static UControlsWidget* CreateInOverlay(UUserWidget* OwnerWidget, TSubclassOf<UControlsWidget> WidgetClass, UOverlay* Overlay);

	void OpenControls();
	void CloseControls();
	bool IsOpen() const;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButtonWidget> CloseButton;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//빈 배경 클릭으로 포커스가 빠지면 ESC가 이 위젯에 오지 않으므로 클릭을 소비하고 포커스 유지
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION()
	void OnCloseButtonClicked();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
