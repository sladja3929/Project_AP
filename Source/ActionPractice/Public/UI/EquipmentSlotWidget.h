#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"
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

	//현재 장착 아이템 이름
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> UsableItemNameText;

	//다음 아이템 프리뷰 슬롯 1 (EquippedIndex+1)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> NextUsableItemIcon1;

	//다음 아이템 프리뷰 슬롯 2 (EquippedIndex+2)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> NextUsableItemIcon2;

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

	//슬롯 전환 연출 — 아이콘이 StartScale에서 커지며 나타나고 이름은 옆에서 미끄러져 들어옴
	UPROPERTY(EditDefaultsOnly, Category = "Switch Animation", meta = (ClampMin = "0.01"))
	float SwitchAnimDuration = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "Switch Animation", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SwitchAnimStartScale = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Switch Animation")
	float SwitchAnimNameOffset = -12.f;

#pragma endregion

#pragma region "Protected Functions"

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void RefreshWeaponSlot(bool bIsLeftHand);

	UFUNCTION()
	void RefreshItemSlot();

	//개별 이미지 갱신 헬퍼
	void SetSlotIcon(UImage* TargetImage, const UBaseItemDataAsset* ItemDA);

#pragma endregion

private:
#pragma region "Private Variables"

	//진행 중인 슬롯 아이콘 비동기 로드 요청 (이미지별)
	//Handle: 로드 중 텍스처 GC 방지 + 슬롯 재사용/위젯 파괴 시 취소용
	//Path: 오래된 콜백이 새 아이콘을 덮어쓰지 않도록 판별하는 세대 검증용
	struct FSlotIconRequest
	{
		TSharedPtr<FStreamableHandle> Handle;
		FSoftObjectPath Path;
	};

	TMap<UImage*, FSlotIconRequest> PendingIconRequests;

	//진행 중인 전환 연출 (대상 위젯별 하나)
	struct FSwitchAnim
	{
		TWeakObjectPtr<UWidget> Target;
		float Elapsed = 0.f;
		float StartScale = 1.f;
		FVector2D StartOffset = FVector2D::ZeroVector;

		//연출 시작 전 디자이너 값 — 연출은 이 값 기준으로 적용하고 끝나면 이 값으로 복귀
		float BaseOpacity = 1.f;
		FWidgetTransform BaseTransform;

		//아이콘 비동기 로드가 끝날 때까지 시작 포즈로 대기 (빈 텍스처에 연출이 재생되지 않도록)
		bool bWaitingForIcon = false;
		float WaitElapsed = 0.f;
	};

	//아이콘 로드가 실패·취소되어도 연출이 멈춰 있지 않도록 하는 대기 상한(초)
	static constexpr float MaxIconWaitTime = 1.0f;

	TArray<FSwitchAnim> ActiveSwitchAnims;

	//실제로 장비가 바뀌었을 때만 연출하기 위한 직전 표시 값 (수량 변화·재바인딩은 제외)
	TWeakObjectPtr<const UBaseItemDataAsset> LastUsableItemDA = nullptr;
	TWeakObjectPtr<const UBaseItemDataAsset> LastLeftWeaponDA = nullptr;
	TWeakObjectPtr<const UBaseItemDataAsset> LastRightWeaponDA = nullptr;

	//SetDataSources의 초기 갱신 중에는 연출 생략
	bool bSuppressSwitchAnim = false;

#pragma endregion

#pragma region "Private Functions"

	//대상 이미지에 진행 중이던 아이콘 비동기 로드가 있으면 취소
	void CancelPendingIconLoad(UImage* TargetImage);

	//아이콘이 없는 슬롯에 빈 텍스처(없으면 Collapsed) 적용
	void ApplyEmptySlotIcon(UImage* TargetImage);

	//대상 위젯 전환 연출 시작 (같은 대상이 진행 중이면 처음부터 다시)
	void PlaySwitchAnim(UWidget* Target, float StartScale, const FVector2D& StartOffset);

	//연출 진행·종료 처리
	void TickSwitchAnims(float DeltaTime);

	//아이콘 로드 완료 — 대기 중이던 해당 이미지의 연출 시작
	void ReleaseSwitchAnimWait(UWidget* Target);

	//진행 중인 연출을 모두 기본 포즈로 되돌리고 정리
	void FinishAllSwitchAnims();

	//진행률 Alpha(0~1)를 대상 위젯의 RenderTransform·RenderOpacity에 반영
	static void ApplySwitchAnimPose(UWidget* Target, const FSwitchAnim& Anim, float Alpha);

#pragma endregion
};
