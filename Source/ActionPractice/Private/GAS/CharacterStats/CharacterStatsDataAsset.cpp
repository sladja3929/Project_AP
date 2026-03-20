#include "GAS/CharacterStats/CharacterStatsDataAsset.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogCharacterStatsDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogCharacterStatsDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UCharacterStatsDataAsset::ApplyInitialAttributes(UAbilitySystemComponent* ASC) const
{
	if (!ASC)
	{
		DEBUG_LOG(TEXT("ApplyInitialAttributes: ASC is nullptr"));
		return;
	}

	for (const FAttributeInitEntry& Entry : AttributeInitValues)
	{
		if (!Entry.Attribute.IsValid())
		{
			DEBUG_LOG(TEXT("ApplyInitialAttributes: Invalid attribute entry, skipping"));
			continue;
		}

		//BaseValue를 직접 설정 (GE 파이프라인을 타지 않음)
		ASC->SetNumericAttributeBase(Entry.Attribute, Entry.Value);

		DEBUG_LOG(TEXT("ApplyInitialAttributes: %s = %.2f"), *Entry.Attribute.GetName(), Entry.Value);
	}
}
