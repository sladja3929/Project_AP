#pragma once

struct FGameplayTag;
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

	//모든 TaggedAttackData 배열 반환 (WeaponDataAsset 내부 TArray 참조)
	static const TArray<FTaggedAttackData>* GetAllAttackDataFromAbility(const UGameplayAbility* Ability, bool bIsLeft = false);

	//특정 태그로 AttackData 검색 (태그 직접 전달 버전)
	static const FTaggedAttackData* GetAttackDataByTag(const UWeaponDataAsset* WeaponDataAsset, const FGameplayTag& AttackTypeTag);

	//인덱스로 AttackData 검색
	static const FTaggedAttackData* GetAttackDataByIndex(const UWeaponDataAsset* WeaponDataAsset, int32 Index);

	static const FTaggedAttackData* GetAttackDataFromAbility(const UGameplayAbility* Ability);
	static const FBlockActionData* GetBlockDataFromAbility(const UGameplayAbility* Ability);
	
};
