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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthDamageBar;

#pragma endregion

#pragma region "Public Functions"

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetAttributeSet(UEnemyAttributeSet* InAttributeSet);

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UEnemyAttributeSet> CachedAttributeSet;

	float CurrentHealthPercent = 1.0f;
	float TargetHealthDamagePercent = 1.0f;
	float HealthDamageDelayTimer = 0.0f;

#pragma endregion

#pragma region "Protected Functions"

	void UpdateDamageBars(float DeltaTime);

#pragma endregion

private:
#pragma region "Private Variables"

	//지연 바 줄어드는 속도
	UPROPERTY(EditAnywhere, Category = "UI Settings", meta = (AllowPrivateAccess = "true"))
	float DamageBarLerpSpeed = 2.0f;

	//지연 바가 몇초 뒤에 줄어드는지
	UPROPERTY(EditAnywhere, Category = "UI Settings", meta = (AllowPrivateAccess = "true"))
	float DamageBarDelayTime = 0.5f;

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
