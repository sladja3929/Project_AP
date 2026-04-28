#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "GameplayTagContainer.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWeaponAbilityStatics, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWeaponAbilityStatics, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

AWeapon* FWeaponAbilityStatics::GetWeaponFromAbility(const UGameplayAbility* Ability, bool bIsLeft)
{
	AActionPracticeCharacter* Character = Cast<AActionPracticeCharacter>(Ability->GetActorInfo().AvatarActor.Get());
	return bIsLeft ? Character->GetLeftWeapon() : Character->GetRightWeapon();
}

const UWeaponDataAsset* FWeaponAbilityStatics::GetWeaponDataAssetFromAbility(const UGameplayAbility* Ability, bool bIsLeft)
{
	AWeapon* Weapon = GetWeaponFromAbility(Ability, bIsLeft);
	if (!Weapon)
	{
		DEBUG_LOG(TEXT("GetWeaponDataAssetFromAbility: No Weapon"));
		return nullptr;
	}

	const UWeaponDataAsset* WeaponDataAsset = Weapon->GetWeaponData();
	if (!WeaponDataAsset)
	{
		DEBUG_LOG(TEXT("GetWeaponDataAssetFromAbility: No WeaponDataAsset"));
		return nullptr;
	}

	return WeaponDataAsset;
}

const FTaggedAttackData* FWeaponAbilityStatics::GetAttackDataByTags(const UWeaponDataAsset* WeaponDataAsset, const FGameplayTagContainer& AttackTags)
{
	if (!WeaponDataAsset || !AttackTags.IsValid())
	{
		DEBUG_LOG(TEXT("GetAttackDataByTagsFromList: Invalid parameters"));
		return nullptr;
	}

	for (const FTaggedAttackData& AttackData : WeaponDataAsset->TaggedAttackData)
	{
		if (AttackData.AttackTags.HasAllExact(AttackTags))
		{
			return &AttackData;
		}
	}

	//DEBUG_LOG(TEXT("GetAttackDataByTagsFromList: No matching AttackData for tag %s"), *AttackTags.ToString());
	return nullptr;
}

const FTaggedAttackData* FWeaponAbilityStatics::GetAttackDataByTagsFromAbility(const UGameplayAbility* Ability)
{
	AWeapon* Weapon = GetWeaponFromAbility(Ability, false);
	if (!Weapon)
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No Weapon"))
		return nullptr;
	}

	FGameplayTagContainer AssetTag = Ability->GetAssetTags();
	if (AssetTag.IsEmpty())
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No AssetTags"))
		return nullptr;
	}

	//몽타주 검증
	const FTaggedAttackData* WeaponAttackData = Weapon->GetWeaponAttackDataByTag(AssetTag);
	if (!WeaponAttackData)
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No AttackData"))
		return nullptr;
	}

	if (WeaponAttackData->ComboSequence.IsEmpty() || !WeaponAttackData->ComboSequence[0].AttackMontage)
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No Attack Montage"))
		return nullptr;
	}

	return WeaponAttackData;
}

const FBlockActionData* FWeaponAbilityStatics::GetBlockDataFromAbility(const UGameplayAbility* Ability)
{
	AWeapon* Weapon = GetWeaponFromAbility(Ability, true);
	if (!Weapon)
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No Weapon"))
		return nullptr;
	}
	
	//몽타주 검증
	const FBlockActionData* WeaponBlockData = Weapon->GetWeaponBlockData();
	if (!WeaponBlockData)
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No BlockData"))
		return nullptr;
	}

	if (WeaponBlockData->BlockIdleMontage.IsNull() || WeaponBlockData->BlockReactionLightMontage.IsNull() ||
		WeaponBlockData->BlockReactionMiddleMontage.IsNull() || WeaponBlockData->BlockReactionHeavyMontage.IsNull())
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No Block Montages"))
		return nullptr;
	}

	return WeaponBlockData;
}
