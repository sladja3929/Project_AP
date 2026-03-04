#include "Public/Characters/LockOnComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogLockOnComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogLockOnComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ULockOnComponent::ULockOnComponent()
{
	SetIsReplicatedByDefault(true);
}

void ULockOnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULockOnComponent, bIsLockOn);
	DOREPLIFETIME(ULockOnComponent, LockedOnTarget);
}

void ULockOnComponent::SetLockedOnTarget(AActor* NewTarget)
{
	bIsLockOn = (NewTarget != nullptr);
	LockedOnTarget = NewTarget;

	if (bIsLockOn)
	{
		SetRotationMode(false, true);
		DEBUG_LOG(TEXT("Lock-On Target: %s"), *NewTarget->GetName());
	}
	else
	{
		SetRotationMode(true, false);
		DEBUG_LOG(TEXT("Lock-On Released"));
	}

	if (!GetOwner()->HasAuthority())
	{
		ServerSetLockOnState(bIsLockOn, LockedOnTarget);
	}
}

void ULockOnComponent::ServerSetLockOnState_Implementation(bool bNewLockOn, AActor* NewTarget)
{
	bIsLockOn = bNewLockOn;
	LockedOnTarget = NewTarget;

	DEBUG_LOG(TEXT("Server: Lock-On State Updated - bIsLockOn: %s, Target: %s"),
		bNewLockOn ? TEXT("true") : TEXT("false"),
		NewTarget ? *NewTarget->GetName() : TEXT("None"));
}

void ULockOnComponent::SetRotationMode(bool bOrientToMovement, bool bUseControllerDesired)
{
	ACharacter* OwnerCharacter = GetOwner<ACharacter>();
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) return;

	CMC->bOrientRotationToMovement = bOrientToMovement;
	CMC->bUseControllerDesiredRotation = bUseControllerDesired;

	if (bCachedOrientToMovement != bOrientToMovement || bCachedUseControllerDesired != bUseControllerDesired)
	{
		bCachedOrientToMovement = bOrientToMovement;
		bCachedUseControllerDesired = bUseControllerDesired;

		if (!OwnerCharacter->HasAuthority())
		{
			ServerSetRotationMode(bOrientToMovement, bUseControllerDesired);
		}
	}
}

void ULockOnComponent::ServerSetRotationMode_Implementation(bool bOrientToMovement, bool bUseControllerDesired)
{
	ACharacter* OwnerCharacter = GetOwner<ACharacter>();
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) return;

	CMC->bOrientRotationToMovement = bOrientToMovement;
	CMC->bUseControllerDesiredRotation = bUseControllerDesired;

	DEBUG_LOG(TEXT("Server: RotationMode Updated - OrientToMovement: %s, UseControllerDesired: %s"),
		bOrientToMovement ? TEXT("true") : TEXT("false"),
		bUseControllerDesired ? TEXT("true") : TEXT("false"));
}

AActor* ULockOnComponent::FindNearestTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	TArray<AActor*> FoundTargets;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TargetActorTag, FoundTargets);

	AActor* NearestTarget = nullptr;
	float NearestDistance = FLT_MAX;

	for (AActor* PotentialTarget : FoundTargets)
	{
		float Distance = FVector::Dist(Owner->GetActorLocation(), PotentialTarget->GetActorLocation());
		if (Distance < NearestDistance && Distance < LockOnMaxDistance)
		{
			NearestDistance = Distance;
			NearestTarget = PotentialTarget;
		}
	}

	return NearestTarget;
}

void ULockOnComponent::ToggleLockOn()
{
	if (bIsLockOn)
	{
		SetLockedOnTarget(nullptr);
	}
	else
	{
		AActor* NearestTarget = FindNearestTarget();
		if (NearestTarget)
		{
			SetLockedOnTarget(NearestTarget);
		}
	}
}
