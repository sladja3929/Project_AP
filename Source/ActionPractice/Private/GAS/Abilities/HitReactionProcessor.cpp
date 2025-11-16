#include "GAS/Abilities/HitReactionProcessor.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogHitReactionProcessor, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogHitReactionProcessor, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

FHitReactionProcessor::FHitReactionProcessor()
{
}

void FHitReactionProcessor::InitReactionLevel(float Light, float Heavy)
{
	LightThreshold = Light;
	HeavyThreshold = Heavy;
	DEBUG_LOG(TEXT("InitReactionLevel: Light=%.1f, Heavy=%.1f"), Light, Heavy);
}

void FHitReactionProcessor::SelectReactionLevel(float PoiseDamage)
{
	//Poise 값에 따라 레벨 선택
	if (PoiseDamage < HeavyThreshold)
	{
		DEBUG_LOG(TEXT("Heavy hit reaction (Poise=%.1f < %.1f)"), PoiseDamage, HeavyThreshold);
		ReactionLevel = EReactionLevel::Heavy;
	}
	else if (PoiseDamage < LightThreshold)
	{
		DEBUG_LOG(TEXT("Middle hit reaction (%.1f <= Poise=%.1f < %.1f)"), HeavyThreshold, PoiseDamage, LightThreshold);
		ReactionLevel = EReactionLevel::Middle;
	}
	else
	{
		DEBUG_LOG(TEXT("Light hit reaction (Poise=%.1f >= %.1f)"), PoiseDamage, LightThreshold);
		ReactionLevel = EReactionLevel::Light;
	}
}