#include "UI/NotificationEntryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Items/BaseItemDataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "TimerManager.h"

#define ENABLE_DEBUG_LOG 0

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
		//이전 아이콘 로드 취소 (위젯 재사용 대비 — 오래된 콜백이 새 아이콘을 덮어쓰지 않도록)
		if (IconLoadHandle.IsValid())
		{
			IconLoadHandle->CancelHandle();
			IconLoadHandle.Reset();
		}

		if (InItemDA->Icon.IsNull())
		{
			NotificationIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			const TSoftObjectPtr<UTexture2D> SoftIcon = InItemDA->Icon;

			//이미 로드된 경우 즉시 반영
			if (UTexture2D* Loaded = SoftIcon.Get())
			{
				NotificationIcon->SetBrushFromTexture(Loaded);
				NotificationIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else if (UAssetManager::IsInitialized())
			{
				//로드 중에는 숨김, 완료 콜백에서 반영
				NotificationIcon->SetVisibility(ESlateVisibility::Collapsed);

				TWeakObjectPtr<UNotificationEntryWidget> WeakThis(this);

				FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
				IconLoadHandle = StreamableManager.RequestAsyncLoad(
					SoftIcon.ToSoftObjectPath(),
					FStreamableDelegate::CreateLambda([WeakThis, SoftIcon]()
					{
						UNotificationEntryWidget* StrongThis = WeakThis.Get();

						//위젯이 파괴됐으면 조용히 반환
						if (!StrongThis || !StrongThis->NotificationIcon)
						{
							return;
						}

						if (UTexture2D* Texture = SoftIcon.Get())
						{
							StrongThis->NotificationIcon->SetBrushFromTexture(Texture);
							StrongThis->NotificationIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
						}
					}),
					FStreamableManager::DefaultAsyncLoadPriority);
			}
			else
			{
				NotificationIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	DEBUG_LOG(TEXT("SetupItemAcquisition: %s x%d"), *InItemDA->DisplayName.ToString(), InCount);
}

void UNotificationEntryWidget::NativeDestruct()
{
	//진행 중인 아이콘 비동기 로드 취소 (파괴 후 콜백이 파괴된 위젯을 건드리지 않도록)
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	Super::NativeDestruct();
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
