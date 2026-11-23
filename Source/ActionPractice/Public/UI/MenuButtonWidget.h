#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuButtonWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuButtonClicked);

//타이틀·일시정지·조작법 메뉴 공용 텍스트 버튼
//배경 없는 텍스트 + 호버 시 텍스트 밝기 변화와 장식(HoverHighlight) 표시로 메뉴 톤을 통일
//폭·폰트·색은 WBP_MenuButton 한 곳에서 관리하고, 각 메뉴는 LabelText만 지정
UCLASS()
class ACTIONPRACTICE_API UMenuButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//버튼 클릭 알림 — 소유 메뉴 위젯이 바인딩
	UPROPERTY(BlueprintAssignable, Category = "Menu Button")
	FOnMenuButtonClicked OnMenuButtonClicked;

#pragma endregion

#pragma region "Public Functions"

	UFUNCTION(BlueprintCallable, Category = "Menu Button")
	void SetLabelText(const FText& InText);

	//내부 버튼에 키보드 포커스 지정 (ESC 등 키 입력이 이 버튼의 상위 위젯으로 전달되도록)
	void FocusButton();

#pragma endregion

protected:
#pragma region "Protected Variables"

	//표시할 메뉴 이름 — 배치한 WBP 인스턴스마다 지정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu Button")
	FText LabelText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu Button|Style")
	FLinearColor NormalTextColor = FLinearColor(0.62f, 0.60f, 0.56f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu Button|Style")
	FLinearColor HoveredTextColor = FLinearColor(1.f, 0.93f, 0.78f, 1.f);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

	//호버 시에만 보이는 장식 (반투명 띠·장식선 등)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HoverHighlight;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"

	void ApplyHoverState(bool bHovered);

#pragma endregion
};
