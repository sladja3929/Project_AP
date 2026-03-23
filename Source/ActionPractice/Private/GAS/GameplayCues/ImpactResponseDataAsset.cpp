#include "GAS/GameplayCues/ImpactResponseDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogImpactResponseDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogImpactResponseDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

const FImpactResponseData& UImpactResponseDataAsset::GetResponse(EPhysicalSurface SurfaceType, bool bIsBlocked) const
{
	if (bIsBlocked)
	{
		DEBUG_LOG(TEXT("GetResponse - Blocked → BlockedResponse"));
		return BlockedResponse;
	}

	const FImpactResponseData* Found = NormalResponses.Find(SurfaceType);
	if (Found)
	{
		DEBUG_LOG(TEXT("GetResponse - SurfaceType: %d → Matched"), static_cast<int32>(SurfaceType));
		return *Found;
	}

	DEBUG_LOG(TEXT("GetResponse - SurfaceType: %d not found → DefaultNormalResponse"), static_cast<int32>(SurfaceType));
	return DefaultNormalResponse;
}
