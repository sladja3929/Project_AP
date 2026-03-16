#include "Public/Characters/LockOnComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Components/WidgetComponent.h"

#define ENABLE_DEBUG_LOG 0

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

void ULockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	//로컬 플레이어에게만 마커 위젯 컴포넌트 생성
	APawn* OwnerPawn = GetOwner<APawn>();
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;
	if (!LockOnMarkerWidgetClass) return;

	LockOnMarkerWidget = NewObject<UWidgetComponent>(GetOwner(), UWidgetComponent::StaticClass(), TEXT("LockOnMarker"));
	LockOnMarkerWidget->SetWidgetClass(LockOnMarkerWidgetClass);
	LockOnMarkerWidget->SetWidgetSpace(EWidgetSpace::Screen);
	LockOnMarkerWidget->SetDrawSize(LockOnMarkerDrawSize);
	LockOnMarkerWidget->SetVisibility(false);
	LockOnMarkerWidget->RegisterComponent();
	LockOnMarkerWidget->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
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
		ShowLockOnMarker(NewTarget);
		DEBUG_LOG(TEXT("Lock-On Target: %s"), *NewTarget->GetName());
	}
	else
	{
		SetRotationMode(true, false);
		HideLockOnMarker();
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

void ULockOnComponent::ShowLockOnMarker(AActor* Target)
{
	if (!LockOnMarkerWidget || !Target) return;

	//스켈레탈 메시 소켓에 부착 시도, 실패 시 루트 컴포넌트로 폴백
	USceneComponent* AttachTarget = Target->GetRootComponent();
	if (!LockOnMarkerSocketName.IsNone())
	{
		if (ACharacter* TargetCharacter = Cast<ACharacter>(Target))
		{
			USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();
			if (Mesh && Mesh->DoesSocketExist(LockOnMarkerSocketName))
			{
				AttachTarget = Mesh;
			}
		}
	}

	FAttachmentTransformRules AttachRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	LockOnMarkerWidget->AttachToComponent(AttachTarget, AttachRules, LockOnMarkerSocketName);
	LockOnMarkerWidget->SetRelativeLocation(LockOnMarkerOffset);
	LockOnMarkerWidget->SetVisibility(true);
}

void ULockOnComponent::HideLockOnMarker()
{
	if (!LockOnMarkerWidget) return;

	LockOnMarkerWidget->SetVisibility(false);
	LockOnMarkerWidget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	LockOnMarkerWidget->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
}
