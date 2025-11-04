#pragma once

struct FBlockActionData;
struct FTaggedAttackData;
class UGameplayAbility;
class AWeapon;

class FWeaponAbilityStatics
{
public:
	static AWeapon* GetWeaponFromAbility(const UGameplayAbility* Ability, bool bIsLeft);
	static const FTaggedAttackData* GetAttackDataFromAbility(const UGameplayAbility* Ability);
	static const FBlockActionData* GetBlockDataFromAbility(const UGameplayAbility* Ability);
};
