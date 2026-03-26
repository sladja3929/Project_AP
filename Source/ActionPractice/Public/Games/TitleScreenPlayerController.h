#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TitleScreenPlayerController.generated.h"

class UTitleScreenWidget;

//타이틀 화면 전용 PlayerController
//UI 생성 및 마우스 커서 활성화 담당
UCLASS()
class ACTIONPRACTICE_API ATitleScreenPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"
#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTitleScreenWidget> TitleScreenWidgetClass;

	UPROPERTY()
	TObjectPtr<UTitleScreenWidget> TitleScreenWidget = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
