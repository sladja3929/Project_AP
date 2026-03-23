#include "GAS/GameplayCues/APGameplayCueNotify_Instant.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAPGameplayCueNotify_Instant, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAPGameplayCueNotify_Instant, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Constructor"

UAPGameplayCueNotify_Instant::UAPGameplayCueNotify_Instant()
{
}

#pragma endregion

#pragma region "GameplayCueNotify_Static Overrides"

bool UAPGameplayCueNotify_Instant::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	DEBUG_LOG(TEXT("OnExecute - Target: %s"), MyTarget ? *MyTarget->GetName() : TEXT("NULL"));

	if (!MyTarget)
	{
		return false;
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;
	GetSpawnTransform(MyTarget, Parameters, SpawnLocation, SpawnRotation);

	//Niagara 원샷 스폰
	if (NiagaraEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MyTarget->GetWorld(),
			NiagaraEffect,
			SpawnLocation,
			SpawnRotation,
			EffectScale,
			true,  //bAutoDestroy
			true,  //bAutoActivate
			ENCPoolMethod::None
		);

		DEBUG_LOG(TEXT("OnExecute - Niagara Spawned: %s at %s"),
			*NiagaraEffect->GetName(),
			*SpawnLocation.ToString());
	}

	//사운드 원샷 재생
	if (InstantSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			MyTarget,
			InstantSound,
			SpawnLocation
		);
	}

	return false;
}

#pragma endregion

#pragma region "Extension Points"

void UAPGameplayCueNotify_Instant::GetSpawnTransform_Implementation(AActor* TargetActor, const FGameplayCueParameters& Parameters, FVector& OutLocation, FRotator& OutRotation) const
{
	if (!TargetActor)
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}

	if (bAttachToSocket)
	{
		USkeletalMeshComponent* MeshComp = TargetActor->FindComponentByClass<USkeletalMeshComponent>();
		if (MeshComp && MeshComp->DoesSocketExist(AttachSocketName))
		{
			FTransform SocketTransform = MeshComp->GetSocketTransform(AttachSocketName);
			OutLocation = SocketTransform.GetLocation() + SocketTransform.GetRotation().RotateVector(LocationOffset);
			OutRotation = SocketTransform.GetRotation().Rotator() + RotationOffset;
			return;
		}
	}

	//폴백: 액터 위치 + 오프셋
	OutLocation = TargetActor->GetActorLocation() + LocationOffset;
	OutRotation = TargetActor->GetActorRotation() + RotationOffset;
}

#pragma endregion
