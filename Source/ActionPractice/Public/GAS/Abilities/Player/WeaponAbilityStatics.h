#pragma once

struct FGameplayTag;
struct FGameplayTagContainer;
class UWeaponDataAsset;
struct FBlockActionData;
struct FTaggedAttackData;
class UGameplayAbility;
class AWeapon;

class FWeaponAbilityStatics
{
public:
	//Weapon 가져오기
	static AWeapon* GetWeaponFromAbility(const UGameplayAbility* Ability, bool bIsLeft);
	
	//WeaponDataAsset 전체 반환
	static const UWeaponDataAsset* GetWeaponDataAssetFromAbility(const UGameplayAbility* Ability, bool bIsLeft = false);

	//태그 컨테이너로 AttackData 검색
	static const FTaggedAttackData* GetAttackDataByTags(const UWeaponDataAsset* WeaponDataAsset, const FGameplayTagContainer& AttackTags);

	static const FTaggedAttackData* GetAttackDataByTagsFromAbility(const UGameplayAbility* Ability);
	static const FBlockActionData* GetBlockDataFromAbility(const UGameplayAbility* Ability);
	
};
