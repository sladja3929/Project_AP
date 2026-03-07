#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UWeaponManagerComponent;
class UItemManagerComponent;
class UBaseItemDataAsset;
class UTexture2D;

UCLASS()
class ACTIONPRACTICE_API UEquipmentSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//좌무기 슬롯
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LeftWeaponIcon;

	//우무기 슬롯
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> RightWeaponIcon;

	//사용 아이템 슬롯
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> UsableItemIcon;

	//사용 아이템 수량
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> UsableItemCountText;

#pragma endregion

#pragma region "Public Functions"

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//데이터 소스 연결 (BeginPlay에서 호출)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetDataSources(UWeaponManagerComponent* InWeaponManager, UItemManagerComponent* InItemManager);

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UWeaponManagerComponent> WeaponManager = nullptr;

	UPROPERTY()
	TObjectPtr<UItemManagerComponent> ItemManager = nullptr;

	//빈 슬롯용 기본 텍스처
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTexture2D> EmptySlotTexture = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	UFUNCTION()
	void RefreshWeaponSlot(bool bIsLeftHand);

	UFUNCTION()
	void RefreshItemSlot();

	//개별 이미지 갱신 헬퍼
	void SetSlotIcon(UImage* TargetImage, const UBaseItemDataAsset* ItemDA);

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
