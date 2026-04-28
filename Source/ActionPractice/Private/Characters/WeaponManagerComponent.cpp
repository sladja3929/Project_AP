#include "Public/Characters/WeaponManagerComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/WeaponEnums.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "Characters/HitDetection/WeaponAttackComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"

#define ENABLE_DEBUG_LOG 0

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

	//디버그 트레이스 "1" 키 바인딩 (로컬 플레이어 한정, 1회만 - 무기 스위치로 재바인딩되지 않음)
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->InputComponent)
		{
			PC->InputComponent->BindKey(EKeys::One, IE_Pressed, this, &UWeaponManagerComponent::ToggleWeaponDebugTrace);
		}
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
		NewWeapon->AttachToCharacterHandByGripSocket(OwnerCharacter->GetMesh(), SocketName);

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

		//Listen Server에서는 OnRep이 호출되지 않으므로 직접 레이어 교체
		if (OwnerCharacter->IsLocallyControlled())
		{
			AWeapon* EquippedWeapon = bIsLeftHand ? LeftWeapon.Get() : RightWeapon.Get();
			UpdateAnimationLayer(EquippedWeapon, bIsLeftHand);
		}

		//새 무기에 현재 디버그 상태 주입 (리스닝 서버/싱글 플레이 경로)
		ApplyDebugTraceToCurrentWeapons();

		DEBUG_LOG(TEXT("EquipWeapon: Weapon Changed, IsLeft=%d"), bIsLeftHand);
		OnWeaponChanged.Broadcast(bIsLeftHand);
	}
}

void UWeaponManagerComponent::UnequipWeapon(bool bIsLeftHand)
{
	TObjectPtr<AWeapon>& WeaponToRemove = bIsLeftHand ? LeftWeapon : RightWeapon;

	if (WeaponToRemove)
	{
		//맨손 상태로 돌아갈 때 레이어 Unlink
		AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
		if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
		{
			UnlinkAnimationLayer(bIsLeftHand);
		}

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
			LeftWeapon->AttachToCharacterHandByGripSocket(OwnerCharacter->GetMesh(), SocketName);
			DEBUG_LOG(TEXT("OnRep_LeftWeapon: Attached to %s"), *SocketName.ToString());
		}
	}

	UpdateAnimationLayer(LeftWeapon, true);
	OwnerCharacter->TryAutoActivateAttackSequenceAbility();
	ApplyDebugTraceToCurrentWeapons();
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
			RightWeapon->AttachToCharacterHandByGripSocket(OwnerCharacter->GetMesh(), SocketName);
			DEBUG_LOG(TEXT("OnRep_RightWeapon: Attached to %s"), *SocketName.ToString());
		}
	}

	UpdateAnimationLayer(RightWeapon, false);
	OwnerCharacter->TryAutoActivateAttackSequenceAbility();
	ApplyDebugTraceToCurrentWeapons();
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

void UWeaponManagerComponent::UpdateAnimationLayer(AWeapon* NewWeapon, bool bIsLeftHand)
{
	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return;

	UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	//무기가 없으면 해당 팔만 Unlink
	if (!NewWeapon)
	{
		UnlinkAnimationLayer(bIsLeftHand);
		return;
	}

	const UWeaponDataAsset* WeaponDA = NewWeapon->GetWeaponData();
	if (!WeaponDA) return;

	//장착 손에 따라 해당 팔의 ABP 클래스 결정
	TSubclassOf<UAnimInstance> LayerClass = bIsLeftHand ? WeaponDA->LeftArmLayerABP : WeaponDA->RightArmLayerABP;

	if (!LayerClass)
	{
		UnlinkAnimationLayer(bIsLeftHand);
		return;
	}

	//해당 팔의 이전 레이어 Unlink 후 새 레이어 Link
	UnlinkAnimationLayer(bIsLeftHand);
	AnimInstance->LinkAnimClassLayers(LayerClass);

	if (bIsLeftHand)
	{
		CurrentLeftArmLayerClass = LayerClass;
	}
	else
	{
		CurrentRightArmLayerClass = LayerClass;
	}

	DEBUG_LOG(TEXT("UpdateAnimationLayer: Linked %s (IsLeft=%d)"), *LayerClass->GetName(), bIsLeftHand);
}

void UWeaponManagerComponent::UnlinkAnimationLayer(bool bIsLeftHand)
{
	TSubclassOf<UAnimInstance>& LayerClassRef = bIsLeftHand ? CurrentLeftArmLayerClass : CurrentRightArmLayerClass;
	if (!LayerClassRef) return;

	AActionPracticeCharacter* OwnerCharacter = GetOwner<AActionPracticeCharacter>();
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return;

	UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	AnimInstance->UnlinkAnimClassLayers(LayerClassRef);
	LayerClassRef = nullptr;

	DEBUG_LOG(TEXT("UnlinkAnimationLayer: Unlinked (IsLeft=%d)"), bIsLeftHand);
}

void UWeaponManagerComponent::ToggleWeaponDebugTrace()
{
	bWeaponDebugTrace = !bWeaponDebugTrace;
	ApplyDebugTraceToCurrentWeapons();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(100, 2.0f, bWeaponDebugTrace ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("[Player] Attack Draw: %s"), bWeaponDebugTrace ? TEXT("ON") : TEXT("OFF")));
	}
}

void UWeaponManagerComponent::ApplyDebugTraceToCurrentWeapons()
{
	auto ApplyTo = [this](AWeapon* Weapon)
	{
		if (!Weapon) return;
		if (UWeaponAttackComponent* TraceComp = Weapon->FindComponentByClass<UWeaponAttackComponent>())
		{
			TraceComp->bDrawDebugTrace = bWeaponDebugTrace;
		}
	};

	ApplyTo(RightWeapon.Get());
	ApplyTo(LeftWeapon.Get());
}
