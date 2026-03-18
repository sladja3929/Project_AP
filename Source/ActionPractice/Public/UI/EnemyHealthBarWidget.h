#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UEnemyAttributeSet;
class UProgressBar;

UCLASS()
class ACTIONPRACTICE_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

#pragma endregion

#pragma region "Public Functions"

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetAttributeSet(UEnemyAttributeSet* InAttributeSet);

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UEnemyAttributeSet> CachedAttributeSet;

#pragma endregion
};
