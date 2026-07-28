#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"
#include "NotificationEntryWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;
class UBaseItemDataAsset;

UCLASS()
class ACTIONPRACTICE_API UNotificationEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"

	//아이템 획득 알림 설정
	void SetupItemAcquisition(const UBaseItemDataAsset* InItemDA, int32 InCount);

	//자동 제거 타이머 시작 — 컨테이너에서 AddChild 이후 호출
	void StartAutoRemoveTimer(float InDuration);

	virtual void NativeDestruct() override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//WBP에서 BindWidget으로 연결
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NotificationText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> NotificationIcon = nullptr;

#pragma endregion

#pragma region "Protected Functions"
#pragma endregion

private:
#pragma region "Private Variables"

	FTimerHandle AutoRemoveTimerHandle;

	//진행 중인 아이콘 비동기 로드 핸들 (로드 중 GC 방지 + 위젯 재사용/파괴 시 취소용)
	TSharedPtr<FStreamableHandle> IconLoadHandle;

#pragma endregion

#pragma region "Private Functions"

	void OnAutoRemoveTimerExpired();

#pragma endregion
};
