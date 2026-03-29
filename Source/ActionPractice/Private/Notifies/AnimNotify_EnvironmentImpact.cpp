// Copyright Epic Games, Inc. All Rights Reserved.

#include "Notifies/AnimNotify_EnvironmentImpact.h"
#include "Effects/EffectManagerSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAnimNotify_EnvironmentImpact, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAnimNotify_EnvironmentImpact, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UAnimNotify_EnvironmentImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !IsValid(Owner))
	{
		return;
	}

	UEffectManagerSubsystem* EffectManager = UEffectManagerSubsystem::Get(Owner);
	if (!EffectManager)
	{
		return;
	}

	//트레이스 시작점 목록 결정
	TArray<FVector> TraceOrigins;

	if (TraceSocketNames.IsEmpty())
	{
		TraceOrigins.Add(Owner->GetActorLocation());
	}
	else
	{
		for (const FName& SocketName : TraceSocketNames)
		{
			if (MeshComp->DoesSocketExist(SocketName))
			{
				TraceOrigins.Add(MeshComp->GetSocketLocation(SocketName));
			}
			else
			{
				DEBUG_LOG(TEXT("Socket [%s] not found, skipping"), *SocketName.ToString());
			}
		}
	}

	//각 시작점마다 트레이스 후 이펙트 스폰
	for (const FVector& Origin : TraceOrigins)
	{
		FVector HitLocation;
		FVector HitNormal;

		if (PerformEnvironmentTrace(MeshComp, Owner, Origin, HitLocation, HitNormal))
		{
			FEnvironmentImpactRequest Request;
			Request.NiagaraEffect = NiagaraEffect;
			Request.Sound = Sound;
			Request.Location = HitLocation;
			Request.Normal = HitNormal;
			Request.LocationOffset = LocationOffset;
			Request.RotationOffset = RotationOffset;
			Request.Scale = Scale;
			Request.bAlignToNormal = bAlignToSurfaceNormal;
			Request.VolumeMultiplier = VolumeMultiplier;
			Request.PitchMultiplier = PitchMultiplier;

			EffectManager->SpawnEnvironmentImpact(Request);
		}
	}
}

bool UAnimNotify_EnvironmentImpact::PerformEnvironmentTrace(
	USkeletalMeshComponent* MeshComp,
	AActor* Owner,
	const FVector& TraceOrigin,
	FVector& OutLocation,
	FVector& OutNormal) const
{
	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return false;
	}

	FVector Direction = GetWorldTraceDirection(Owner);
	FVector TraceEnd = TraceOrigin + (Direction * TraceLength);

	FCollisionQueryParams QueryParams(TEXT("EnvironmentImpact"), false);
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult HitResult;
	bool bHit = false;

	if (TraceRadius > 0.0f)
	{
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(TraceRadius);
		bHit = World->SweepSingleByChannel(
			HitResult,
			TraceOrigin,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			SphereShape,
			QueryParams
		);
	}
	else
	{
		bHit = World->LineTraceSingleByChannel(
			HitResult,
			TraceOrigin,
			TraceEnd,
			TraceChannel,
			QueryParams
		);
	}

	if (bHit)
	{
		OutLocation = HitResult.ImpactPoint;
		OutNormal = HitResult.ImpactNormal;
		return true;
	}

	DEBUG_LOG(TEXT("Environment trace missed from [%s]"), *TraceOrigin.ToString());
	return false;
}

FVector UAnimNotify_EnvironmentImpact::GetWorldTraceDirection(AActor* Owner) const
{
	switch (TraceDirection)
	{
	case EEnvironmentTraceDirection::WorldDown:
		return FVector::DownVector;

	case EEnvironmentTraceDirection::ActorForward:
		return Owner->GetActorForwardVector();

	case EEnvironmentTraceDirection::ActorDown:
		return -Owner->GetActorUpVector();

	case EEnvironmentTraceDirection::Custom:
		{
			FVector WorldDirection = Owner->GetActorTransform().TransformVectorNoScale(CustomTraceDirection);
			return WorldDirection.GetSafeNormal();
		}

	default:
		return FVector::DownVector;
	}
}

FString UAnimNotify_EnvironmentImpact::GetNotifyName_Implementation() const
{
	if (NiagaraEffect)
	{
		return FString::Printf(TEXT("EnvImpact: %s"), *NiagaraEffect->GetName());
	}
	return TEXT("Environment Impact");
}
