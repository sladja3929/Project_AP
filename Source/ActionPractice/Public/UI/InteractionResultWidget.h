#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionResultWidget.generated.h"

class UVerticalBox;
class UOverlay;
class UNotificationEntryWidget;
class UBaseItemDataAsset;

UCLASS()
class ACTIONPRACTICE_API UInteractionResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"

	//아이템 획득 알림 추가 — NotificationEntry를 생성하여 우측 VBox에 스택
	void AddItemAcquisitionNotification(const UBaseItemDataAsset* InItemDA, int32 InCount);

	//향후 확장:
	//void ShowSummonResult(const FText& InMessage);
	//void ShowDescription(const FText& InText);

#pragma endregion

protected:
#pragma region "Protected Variables"

	// ===== 우측 알림 영역 =====
	//알림 Entry들을 스택하는 VerticalBox — WBP에서 화면 우측에 배치
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> NotificationBox = nullptr;

	//동적 생성할 Entry 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Notification")
	TSubclassOf<UNotificationEntryWidget> NotificationEntryWidgetClass;

	//각 알림 Entry의 표시 지속 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Notification")
	float NotificationDuration = 3.0f;

	//동시에 표시할 최대 알림 수 — 초과 시 가장 오래된 Entry 즉시 제거
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Notification")
	int32 MaxVisibleNotifications = 5;

	// ===== 중앙 영역 (향후 확장) =====
	//소환 성공 UI — WBP에서 배치하지 않으면 nullptr 유지
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> SummonResultPanel = nullptr;

	//설명문 UI — WBP에서 배치하지 않으면 nullptr 유지
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> DescriptionPanel = nullptr;

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
