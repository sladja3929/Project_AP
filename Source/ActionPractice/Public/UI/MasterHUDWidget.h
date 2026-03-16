#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MasterHUDWidget.generated.h"

class UOverlay;
class UPlayerStatsWidget;
class UEquipmentSlotWidget;
class UBossHealthWidget;
class UDeathScreenWidget;
class UInteractionPromptWidget;
class UInteractionResultWidget;
class UActionPracticeAttributeSet;
class UBossAttributeSet;
class UWeaponManagerComponent;
class UItemManagerComponent;
class UBaseItemDataAsset;

UCLASS()
class ACTIONPRACTICE_API UMasterHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"

	//플레이어 HUD 데이터 소스 바인딩
	void BindPlayerData(UActionPracticeAttributeSet* InAttributeSet, UWeaponManagerComponent* InWeaponManager, UItemManagerComponent* InItemManager);

	//보스 HP바 표시
	void ShowBossHealth(UBossAttributeSet* InBossAttributeSet, const FName& InBossName);

	//보스 HP바 숨김
	void HideBossHealth();

	//DeathScreen 표시/숨김 래퍼
	void HandleDeadStateStart();
	void HandleDeadStateFinish();
	void SetDeathScreenVisibility(bool bShow);

	//상호작용 프롬프트 표시/숨김
	void ShowInteractionPrompt(const FText& InPromptText);
	void HideInteractionPrompt();

	//상호작용 프롬프트 디밍 제어
	void SetInteractionPromptDimmed(bool bDimmed);

	//아이템 획득 알림 — InteractionResultWidget으로 전달
	void ShowItemAcquisition(const UBaseItemDataAsset* InItemDA, int32 InCount);

	//위젯 접근자 (외부에서 직접 접근이 필요한 경우 대비)
	FORCEINLINE UDeathScreenWidget* GetDeathScreenWidget() const { return DeathScreenWidget; }
	FORCEINLINE UInteractionPromptWidget* GetInteractionPromptWidget() const { return InteractionPromptWidget; }

	//AddToViewport 이후 컨트롤러에서 명시적으로 호출 — NativeConstruct 안에서 호출하지 않음
	void CreateChildWidgets();

#pragma endregion

protected:
#pragma region "Protected Variables"

	//레이어 컨테이너 — WBP에서 BindWidget으로 연결
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> BaseLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> WorldEventLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> ModalLayer;

	//하위 위젯 클래스 — EditDefaultsOnly로 WBP에서 지정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerStatsWidget> PlayerStatsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UEquipmentSlotWidget> EquipmentSlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UBossHealthWidget> BossHealthWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UDeathScreenWidget> DeathScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UInteractionPromptWidget> InteractionPromptWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UInteractionResultWidget> InteractionResultWidgetClass;

	//생성된 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UPlayerStatsWidget> PlayerStatsWidget;

	UPROPERTY()
	TObjectPtr<UEquipmentSlotWidget> EquipmentSlotWidget;

	UPROPERTY()
	TObjectPtr<UBossHealthWidget> BossHealthWidget;

	UPROPERTY()
	TObjectPtr<UDeathScreenWidget> DeathScreenWidget;

	UPROPERTY()
	TObjectPtr<UInteractionPromptWidget> InteractionPromptWidget;

	UPROPERTY()
	TObjectPtr<UInteractionResultWidget> InteractionResultWidget;

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
