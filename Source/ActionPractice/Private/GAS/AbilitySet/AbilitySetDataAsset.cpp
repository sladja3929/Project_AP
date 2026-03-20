#include "GAS/AbilitySet/AbilitySetDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAbilitySetDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAbilitySetDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "FAbilitySetGrantedHandles"

void FAbilitySetGrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FAbilitySetGrantedHandles::AddEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		EffectHandles.Add(Handle);
	}
}

void FAbilitySetGrantedHandles::RemoveFromASC(UAbilitySystemComponent* ASC)
{
	if (!ASC) return;

	//어빌리티 회수
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	//이펙트 회수
	for (const FActiveGameplayEffectHandle& Handle : EffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	AbilitySpecHandles.Empty();
	EffectHandles.Empty();
}

#pragma endregion

#pragma region "UAbilitySetDataAsset"

void UAbilitySetDataAsset::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FAbilitySetGrantedHandles* OutHandles, UObject* SourceObject) const
{
	if (!ASC)
	{
		DEBUG_LOG(TEXT("GiveToAbilitySystem: ASC is nullptr"));
		return;
	}

	//어빌리티 부여
	for (const FAbilitySetEntry_Ability& Entry : GrantedAbilities)
	{
		if (!Entry.Ability)
		{
			DEBUG_LOG(TEXT("GiveToAbilitySystem: Ability entry is nullptr, skipping"));
			continue;
		}

		FGameplayAbilitySpec Spec(Entry.Ability, Entry.AbilityLevel, INDEX_NONE, SourceObject);
		FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);

		if (OutHandles)
		{
			OutHandles->AddAbilitySpecHandle(Handle);
		}

		DEBUG_LOG(TEXT("GiveToAbilitySystem: Granted Ability=%s, Level=%d"), *GetNameSafe(Entry.Ability), Entry.AbilityLevel);
	}

	//이펙트 적용
	for (const FAbilitySetEntry_Effect& Entry : GrantedEffects)
	{
		if (!Entry.Effect)
		{
			DEBUG_LOG(TEXT("GiveToAbilitySystem: Effect entry is nullptr, skipping"));
			continue;
		}

		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		if (SourceObject)
		{
			EffectContext.AddSourceObject(SourceObject);
		}

		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Entry.Effect, Entry.EffectLevel, EffectContext);
		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

			if (OutHandles)
			{
				OutHandles->AddEffectHandle(EffectHandle);
			}

			DEBUG_LOG(TEXT("GiveToAbilitySystem: Applied Effect=%s, Level=%.1f"), *GetNameSafe(Entry.Effect), Entry.EffectLevel);
		}
	}
}

#pragma endregion
