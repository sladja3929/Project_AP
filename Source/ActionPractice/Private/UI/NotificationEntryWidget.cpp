#include "UI/NotificationEntryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Items/BaseItemDataAsset.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogNotificationEntryWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogNotificationEntryWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UNotificationEntryWidget::SetupItemAcquisition(const UBaseItemDataAsset* InItemDA, int32 InCount)
{
	if (!InItemDA) return;

	//텍스트: "아이템 이름 ×수량"
	if (NotificationText)
	{
		if (InCount > 1)
		{
			const FText FormattedText = FText::Format(
				NSLOCTEXT("Notification", "ItemAcquiredFormat", "{0} \u00D7{1}"),
				InItemDA->DisplayName,
				FText::AsNumber(InCount)
			);
			NotificationText->SetText(FormattedText);
		}
		else
		{
			NotificationText->SetText(InItemDA->DisplayName);
		}
	}

	//아이콘
	if (NotificationIcon)
	{
		if (!InItemDA->Icon.IsNull())
		{
			UTexture2D* IconTexture = InItemDA->Icon.LoadSynchronous();
			if (IconTexture)
			{
				NotificationIcon->SetBrushFromTexture(IconTexture);
				NotificationIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else
			{
				NotificationIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			NotificationIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	DEBUG_LOG(TEXT("SetupItemAcquisition: %s x%d"), *InItemDA->DisplayName.ToString(), InCount);
}

void UNotificationEntryWidget::StartAutoRemoveTimer(float InDuration)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AutoRemoveTimerHandle,
			this,
			&UNotificationEntryWidget::OnAutoRemoveTimerExpired,
			InDuration,
			false
		);
	}
}

void UNotificationEntryWidget::OnAutoRemoveTimerExpired()
{
	DEBUG_LOG(TEXT("OnAutoRemoveTimerExpired: Removing entry"));
	RemoveFromParent();
}
