#include "Items/BaseItemDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBaseItemDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseItemDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif
