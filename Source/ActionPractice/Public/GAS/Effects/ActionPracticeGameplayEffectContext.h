#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Items/AttackData.h"
#include "GAS/AbilitySystemComponent/DefensePolicy.h"
#include "ActionPracticeGameplayEffectContext.generated.h"

USTRUCT()
struct ACTIONPRACTICE_API FActionPracticeGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FActionPracticeGameplayEffectContext(): Super(), AttackDamageType(EAttackDamageType::None), PoiseDamage(0.0f), DefenseResult(EDefenseResult::None) {}
	virtual ~FActionPracticeGameplayEffectContext()	{}

	//오버라이드 필수 Functions
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
	virtual FGameplayEffectContext* Duplicate() const override;
	virtual UScriptStruct* GetScriptStruct() const override;

	//Getter
	EAttackDamageType GetAttackDamageType() const {	return AttackDamageType; }
	float GetPoiseDamage() const { return PoiseDamage; }

	//Setter
	void SetAttackDamageType(EAttackDamageType InAttackDamageType) { AttackDamageType = InAttackDamageType;	}
	void SetPoiseDamage(float InPoiseDamage) { PoiseDamage = InPoiseDamage; }
	EDefenseResult GetDefenseResult() const { return DefenseResult; }
	void SetDefenseResult(EDefenseResult InDefenseResult) { DefenseResult = InDefenseResult; }

	//다운캐스팅 헬퍼 함수
	static FActionPracticeGameplayEffectContext* GetActionPracticeEffectContext(FGameplayEffectContextHandle& Handle);
	static const FActionPracticeGameplayEffectContext* GetActionPracticeEffectContext(const FGameplayEffectContextHandle& Handle);

protected:

	UPROPERTY()
	EAttackDamageType AttackDamageType;

	UPROPERTY()
	float PoiseDamage;

	UPROPERTY()
	EDefenseResult DefenseResult = EDefenseResult::None;

};

template<>
struct TStructOpsTypeTraits<FActionPracticeGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FActionPracticeGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};