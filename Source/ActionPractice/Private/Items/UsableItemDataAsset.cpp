#include "Items/UsableItemDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogUsableItemDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogUsableItemDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UUsableItemDataAsset::PreloadMontage()
{
	if (UseMontage.IsNull()) return;
	if (UseMontage.IsValid()) return; //이미 로드됨

	if (UAssetManager::IsInitialized())
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();
		StreamableManager.LoadSynchronous(UseMontage.ToSoftObjectPath());
	}
}
