#include "UI/InteractionResultWidget.h"
#include "UI/NotificationEntryWidget.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Items/BaseItemDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogInteractionResultWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogInteractionResultWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UInteractionResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//중앙 패널들 초기 숨김 (WBP에서 배치된 경우에만)
	if (SummonResultPanel)
	{
		SummonResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DescriptionPanel)
	{
		DescriptionPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInteractionResultWidget::AddItemAcquisitionNotification(const UBaseItemDataAsset* InItemDA, int32 InCount)
{
	if (!InItemDA) return;
	if (!NotificationBox) return;
	if (!NotificationEntryWidgetClass) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	//최대 개수 초과 시 가장 오래된 Entry(첫 번째 자식) 제거
	while (NotificationBox->GetChildrenCount() >= MaxVisibleNotifications)
	{
		UWidget* OldestChild = NotificationBox->GetChildAt(0);
		if (OldestChild)
		{
			OldestChild->RemoveFromParent();
			DEBUG_LOG(TEXT("AddItemAcquisitionNotification: Removed oldest entry (overflow)"));
		}
		else
		{
			break;
		}
	}

	//Entry 생성
	UNotificationEntryWidget* NewEntry = CreateWidget<UNotificationEntryWidget>(PC, NotificationEntryWidgetClass);
	if (!NewEntry) return;

	NewEntry->SetupItemAcquisition(InItemDA, InCount);

	//VBox에 추가
	UVerticalBoxSlot* VBoxSlot = NotificationBox->AddChildToVerticalBox(NewEntry);
	if (VBoxSlot)
	{
		VBoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Right);
	}

	//자동 제거 타이머 시작
	NewEntry->StartAutoRemoveTimer(NotificationDuration);

	DEBUG_LOG(TEXT("AddItemAcquisitionNotification: Entry added — %s x%d"), *InItemDA->DisplayName.ToString(), InCount);
}
