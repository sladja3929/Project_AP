#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthWidget.generated.h"

class UBossAttributeSet;
class UProgressBar;
class UTextBlock;

UCLASS()
class ACTIONPRACTICE_API UBossHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> BossHealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> BossHealthDamageBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BossNameText;

#pragma endregion

#pragma region "Public Functions"

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetBossAttributeSet(UBossAttributeSet* InAttributeSet);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetBossName(const FName& InBossName);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateBossHealth(float CurrentHealth, float MaxHealth);

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UBossAttributeSet> BossAttributeSet;

	float CurrentBossHealthPercent = 1.0f;
	float TargetBossHealthDamagePercent = 1.0f;
	float CurrentBossHealthDelayTimer = 0.0f;

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