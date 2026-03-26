#include "GAS/Effects/ActionPracticeGameplayEffectContext.h"

bool FActionPracticeGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	//AttackDamageType 직렬화
	uint8 DamageTypeValue = static_cast<uint8>(AttackDamageType);
	Ar << DamageTypeValue;

	if (Ar.IsLoading())
	{
		AttackDamageType = static_cast<EAttackDamageType>(DamageTypeValue);
	}

	//PoiseDamage 직렬화
	Ar << PoiseDamage;

	//DefenseResult 직렬화
	uint8 DefenseResultValue = static_cast<uint8>(DefenseResult);
	Ar << DefenseResultValue;
	if (Ar.IsLoading())
	{
		DefenseResult = static_cast<EDefenseResult>(DefenseResultValue);
	}

	//bUnparriable 직렬화
	Ar << bUnparriable;

	bOutSuccess = true;
	return true;
}

FGameplayEffectContext* FActionPracticeGameplayEffectContext::Duplicate() const
{
	FActionPracticeGameplayEffectContext* NewContext = new FActionPracticeGameplayEffectContext();
	*NewContext = *this;
	if (GetHitResult())
	{
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}

UScriptStruct* FActionPracticeGameplayEffectContext::GetScriptStruct() const
{
	return FActionPracticeGameplayEffectContext::StaticStruct();
}

FActionPracticeGameplayEffectContext* FActionPracticeGameplayEffectContext::GetActionPracticeEffectContext(FGameplayEffectContextHandle& Handle)
{
	FGameplayEffectContext* BaseContext = Handle.Get();
	if (BaseContext && BaseContext->GetScriptStruct()->IsChildOf(FActionPracticeGameplayEffectContext::StaticStruct()))
	{
		return static_cast<FActionPracticeGameplayEffectContext*>(BaseContext);
	}

	return nullptr;
}

const FActionPracticeGameplayEffectContext* FActionPracticeGameplayEffectContext::GetActionPracticeEffectContext(const FGameplayEffectContextHandle& Handle)
{
	const FGameplayEffectContext* BaseContext = Handle.Get();
	if (BaseContext && BaseContext->GetScriptStruct()->IsChildOf(FActionPracticeGameplayEffectContext::StaticStruct()))
	{
		return static_cast<const FActionPracticeGameplayEffectContext*>(BaseContext);
	}

	return nullptr;
}
