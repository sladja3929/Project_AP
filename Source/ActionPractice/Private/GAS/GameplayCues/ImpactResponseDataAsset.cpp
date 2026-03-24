#include "GAS/GameplayCues/ImpactResponseDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogImpactResponseDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogImpactResponseDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

const FImpactResponseData& UImpactResponseDataAsset::GetResponse(EPhysicalSurface SurfaceType, EDefenseResult InDefenseResult) const
{
	switch (InDefenseResult)
	{
	case EDefenseResult::GuardBroken:
		DEBUG_LOG(TEXT("GetResponse - GuardBroken → GuardBreakResponse"));
		return GuardBreakResponse;

	case EDefenseResult::Blocked:
		DEBUG_LOG(TEXT("GetResponse - Blocked → BlockedResponse"));
		return BlockedResponse;

	default:
		break;
	}

	//None(일반 피격): SurfaceType 분기
	const FImpactResponseData* Found = NormalResponses.Find(SurfaceType);
	if (Found)
	{
		DEBUG_LOG(TEXT("GetResponse - SurfaceType: %d → Matched"), static_cast<int32>(SurfaceType));
		return *Found;
	}

	DEBUG_LOG(TEXT("GetResponse - SurfaceType: %d not found → DefaultNormalResponse"), static_cast<int32>(SurfaceType));
	return DefaultNormalResponse;
}
