#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/Abilities/Player/ActionPracticeAbility.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"

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

const TArray<FTaggedAttackData>* FWeaponAbilityStatics::GetAllAttackDataFromAbility(const UGameplayAbility* Ability, bool bIsLeft)
{
	const UWeaponDataAsset* WeaponDataAsset = GetWeaponDataAssetFromAbility(Ability, bIsLeft);
	if (!WeaponDataAsset)
	{
		return nullptr;
	}

	if (WeaponDataAsset->TaggedAttackData.IsEmpty())
	{
		DEBUG_LOG(TEXT("GetAllAttackDataFromAbility: TaggedAttackData is empty"));
		return nullptr;
	}

	return &WeaponDataAsset->TaggedAttackData;
}

const FTaggedAttackData* FWeaponAbilityStatics::GetAttackDataByTag(const UWeaponDataAsset* WeaponDataAsset, const FGameplayTag& AttackTypeTag)
{
	if (!WeaponDataAsset || !AttackTypeTag.IsValid())
	{
		DEBUG_LOG(TEXT("GetAttackDataByTag: Invalid parameters"));
		return nullptr;
	}

	for (const FTaggedAttackData& AttackData : WeaponDataAsset->TaggedAttackData)
	{
		if (AttackData.AttackTags.HasTag(AttackTypeTag))
		{
			return &AttackData;
		}
	}

	DEBUG_LOG(TEXT("GetAttackDataByTag: No matching AttackData for tag %s"), *AttackTypeTag.ToString());
	return nullptr;
}

const FTaggedAttackData* FWeaponAbilityStatics::GetAttackDataByIndex(const UWeaponDataAsset* WeaponDataAsset, int32 Index)
{
	if (!WeaponDataAsset)
	{
		DEBUG_LOG(TEXT("GetAttackDataByIndex: No WeaponDataAsset"));
		return nullptr;
	}

	if (!WeaponDataAsset->TaggedAttackData.IsValidIndex(Index))
	{
		DEBUG_LOG(TEXT("GetAttackDataByIndex: Invalid index %d (Array size: %d)"), Index, WeaponDataAsset->TaggedAttackData.Num());
		return nullptr;
	}

	return &WeaponDataAsset->TaggedAttackData[Index];
}

const FTaggedAttackData* FWeaponAbilityStatics::GetAttackDataFromAbility(const UGameplayAbility* Ability)
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

	if (!WeaponBlockData->BlockIdleMontage || !WeaponBlockData->BlockReactionLightMontage ||
		!WeaponBlockData->BlockReactionMiddleMontage || !WeaponBlockData->BlockReactionHeavyMontage)
	{
		DEBUG_LOG(TEXT("WeaponAbilityStatics: No Block Montages"))
		return nullptr;
	}

	return WeaponBlockData;
}
