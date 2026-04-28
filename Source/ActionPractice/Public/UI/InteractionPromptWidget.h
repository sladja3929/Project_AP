#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

class UTextBlock;

UCLASS()
class ACTIONPRACTICE_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"

	//프롬프트 표시 — 텍스트 갱신 + Visible 전환
	void ShowPrompt(const FText& InPromptText);

	//프롬프트 숨김 — Collapsed 전환
	void HidePrompt();

	//Recovering 등 상호작용 불가 시 오파시티 낮춤
	void SetDimmed(bool bDimmed);

#pragma endregion

protected:
#pragma region "Protected Variables"

	//WBP에서 BindWidget으로 연결할 텍스트 블록
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptTextBlock = nullptr;

	//디밍 시 적용할 오파시티 — WBP에서 조정 가능
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prompt")
	float DimmedOpacity = 0.4f;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeConstruct() override;

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
