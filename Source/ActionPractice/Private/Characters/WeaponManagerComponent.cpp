#include "Public/Characters/WeaponManagerComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/WeaponEnums.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWeaponManagerComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWeaponManagerComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UWeaponManagerComponent::UWeaponManagerComponent()
{
	SetIsReplicatedByDefault(true);
}

void UWeaponManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	//첫 번째 무기 장착 (서버에서만, EquipWeapon 내부에서 Authority 체크)
	if (DefaultRightWeapons.Num() > 0)
	{
		EquipWeapon(DefaultRightWeapons[0], false, false);
	}

	if (DefaultLeftWeapons.Num() > 0)
	{
		EquipWeapon(DefaultLeftWeapons[0], true, false);
	}
}

void UWeaponManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponManagerComponent, LeftWeapon);
	DOREPLIFETIME(UWeaponManagerComponent, RightWeapon);
}

TScriptInterface<IHitDetectionInterface> UWeaponManagerComponent::GetHitDetectionInterface() const
{
	if (!RightWeapon) return nullptr;
	return RightWeapon->GetHitDetectionComponent();
}

void UWeaponManagerComponent::EquipWeapon(TSubclassOf<AWeapon> NewWeaponClass, bool bIsLeftHand, bool bIsTwoHanded)
{
	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter) return;

	//서버에서만 실행 (싱글플레이어에서는 HasAuthority()가 항상 true)
	if (!OwnerCharacter->HasAuthority())
	{
		return;
	}

	if (!NewWeaponClass) return;

	if (bIsTwoHanded) UnequipWeapon(!bIsLeftHand);
	UnequipWeapon(bIsLeftHand);

	//새 무기 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter->GetInstigator();

	AWeapon* NewWeapon = GetWorld()->SpawnActor<AWeapon>(NewWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	EWeaponEnums type = NewWeapon->GetWeaponType();

	if (NewWeapon && type != EWeaponEnums::None)
	{
		FName SocketName = ResolveSocketName(type, bIsLeftHand);
		DEBUG_LOG(TEXT("Equiped Weapon: %s"), *SocketName.ToString());
		NewWeapon->AttachToComponent(OwnerCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

		if (bIsTwoHanded)
		{
			LeftWeapon = NewWeapon;
			RightWeapon = NewWeapon;
		}
		else if (bIsLeftHand)
		{
			LeftWeapon = NewWeapon;
		}
		else
		{
			RightWeapon = NewWeapon;
		}

		DEBUG_LOG(TEXT("EquipWeapon: Weapon Changed, IsLeft=%d"), bIsLeftHand);
		OnWeaponChanged.Broadcast(bIsLeftHand);
	}
}

void UWeaponManagerComponent::UnequipWeapon(bool bIsLeftHand)
{
	TObjectPtr<AWeapon>& WeaponToRemove = bIsLeftHand ? LeftWeapon : RightWeapon;

	if (WeaponToRemove)
	{
		WeaponToRemove->Destroy();
		WeaponToRemove = nullptr;
	}
}

TSubclassOf<AWeapon> UWeaponManagerComponent::LoadWeaponClassByName(const FString& WeaponName)
{
	FString BlueprintPath = FString::Printf(TEXT("%s%s.%s_C"),
		*WeaponBlueprintBasePath,
		*WeaponName,
		*WeaponName);

	UClass* LoadedClass = LoadClass<AWeapon>(nullptr, *BlueprintPath);

	if (LoadedClass && LoadedClass->IsChildOf(AWeapon::StaticClass()))
	{
		return TSubclassOf<AWeapon>(LoadedClass);
	}

	DEBUG_LOG(TEXT("Failed to load weapon class from path: %s"), *BlueprintPath);
	return nullptr;
}

void UWeaponManagerComponent::CycleRightWeapon()
{
	if (DefaultRightWeapons.Num() <= 1) return;

	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter) return;

	//서버에서만 실행 (LocalPredicted 어빌리티가 양쪽에서 호출하므로 RPC 불필요)
	if (!OwnerCharacter->HasAuthority()) return;

	RightWeaponIndex = (RightWeaponIndex + 1) % DefaultRightWeapons.Num();
	EquipWeapon(DefaultRightWeapons[RightWeaponIndex], false, false);

	DEBUG_LOG(TEXT("CycleRightWeapon: index %d"), RightWeaponIndex);
}

void UWeaponManagerComponent::CycleLeftWeapon()
{
	if (DefaultLeftWeapons.Num() <= 1) return;

	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter) return;

	if (!OwnerCharacter->HasAuthority()) return;

	LeftWeaponIndex = (LeftWeaponIndex + 1) % DefaultLeftWeapons.Num();
	EquipWeapon(DefaultLeftWeapons[LeftWeaponIndex], true, false);

	DEBUG_LOG(TEXT("CycleLeftWeapon: index %d"), LeftWeaponIndex);
}

void UWeaponManagerComponent::OnRep_LeftWeapon()
{
	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter) return;

	if (LeftWeapon && OwnerCharacter->GetMesh())
	{
		EWeaponEnums type = LeftWeapon->GetWeaponType();
		if (type != EWeaponEnums::None)
		{
			FName SocketName = ResolveSocketName(type, true);
			LeftWeapon->AttachToComponent(OwnerCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			DEBUG_LOG(TEXT("OnRep_LeftWeapon: Attached to %s"), *SocketName.ToString());
		}
	}

	OwnerCharacter->TryAutoActivateAttackSequenceAbility();
	OnWeaponChanged.Broadcast(true);
}

void UWeaponManagerComponent::OnRep_RightWeapon()
{
	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter) return;

	if (RightWeapon && OwnerCharacter->GetMesh())
	{
		EWeaponEnums type = RightWeapon->GetWeaponType();
		if (type != EWeaponEnums::None)
		{
			FName SocketName = ResolveSocketName(type, false);
			RightWeapon->AttachToComponent(OwnerCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			DEBUG_LOG(TEXT("OnRep_RightWeapon: Attached to %s"), *SocketName.ToString());
		}
	}

	OwnerCharacter->TryAutoActivateAttackSequenceAbility();
	OnWeaponChanged.Broadcast(false);
}

FName UWeaponManagerComponent::ResolveSocketName(EWeaponEnums WeaponType, bool bIsLeftHand) const
{
	FString SocketString = bIsLeftHand ? "hand_l" : "hand_r";

	switch (WeaponType)
	{
	case EWeaponEnums::StraightSword:
		SocketString += "_sword";
		break;
	case EWeaponEnums::GreatSword:
		SocketString += "_greatsword";
		break;
	case EWeaponEnums::Shield:
		SocketString += "_shield";
		break;
	default:
		break;
	}

	return FName(*SocketString);
}
