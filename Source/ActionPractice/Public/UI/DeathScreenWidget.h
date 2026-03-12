#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathScreenWidget.generated.h"

UCLASS()
class ACTIONPRACTICE_API UDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UFUNCTION()
	void HandleDeadStateStart();

	UFUNCTION()
	void HandleDeadStateFinish();
	
	UFUNCTION(BlueprintCallable, Category = "DeathScreen")
	void SetDeathScreenVisibility(bool bShow);

#pragma endregion

protected:
#pragma region "Protected Functions"

	//BP 파생 클래스에서 FadeIn 연출 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "DeathScreen")
	void StartFadeIn();

	//BP 파생 클래스에서 FadeOut 연출 구현
	//FadeOut 애니메이션의 Finished 델리게이트에 SetDeathScreenVisibility(false) 바인딩할 것
	UFUNCTION(BlueprintNativeEvent, Category = "DeathScreen")
	void StartFadeOut();
	virtual void StartFadeOut_Implementation();

#pragma endregion
};
