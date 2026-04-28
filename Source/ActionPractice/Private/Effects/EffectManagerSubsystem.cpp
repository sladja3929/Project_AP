// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/EffectManagerSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Camera/CameraShakeBase.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEffectManagerSubsystem, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEffectManagerSubsystem, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UEffectManagerSubsystem* UEffectManagerSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UEffectManagerSubsystem>();
}

UNiagaraComponent* UEffectManagerSubsystem::SpawnEnvironmentImpact(const FEnvironmentImpactRequest& Request)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	if (!Request.NiagaraEffect && !Request.Sound)
	{
		return nullptr;
	}

	//최종 회전 계산
	FRotator FinalRotation = Request.bAlignToNormal
		? CalculateAlignedRotation(Request.Normal, Request.RotationOffset)
		: Request.RotationOffset;

	//최종 위치 계산 (오프셋을 회전된 로컬 공간으로 변환)
	FVector FinalLocation = Request.Location + FinalRotation.RotateVector(Request.LocationOffset);

	//Niagara 스폰
	UNiagaraComponent* SpawnedComponent = nullptr;
	if (Request.NiagaraEffect)
	{
		SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			Request.NiagaraEffect,
			FinalLocation,
			FinalRotation,
			Request.Scale,
			true,  //bAutoDestroy
			true,  //bAutoActivate
			ENCPoolMethod::None,
			true   //bPreCullCheck
		);
	}

	//사운드 스폰
	if (Request.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World,
			Request.Sound,
			FinalLocation,
			Request.VolumeMultiplier,
			Request.PitchMultiplier
		);
	}

	//카메라 쉐이크 재생
	if (Request.CameraShakeClass)
	{
		UGameplayStatics::PlayWorldCameraShake(
			World,
			Request.CameraShakeClass,
			FinalLocation,
			Request.CameraShakeInnerRadius,
			Request.CameraShakeOuterRadius,
			Request.CameraShakeFalloff,
			false  //bOrientShakeTowardsEpicenter
		);
	}

	return SpawnedComponent;
}

FRotator UEffectManagerSubsystem::CalculateAlignedRotation(const FVector& Normal, const FRotator& AdditionalRotation) const
{
	if (Normal.IsNearlyZero())
	{
		return AdditionalRotation;
	}

	//Z축이 Normal을 향하도록 회전 행렬 생성
	FRotator BaseRotation = FRotationMatrix::MakeFromZ(Normal).Rotator();

	//추가 회전 합산 (쿼터니언으로 안전하게)
	return (BaseRotation.Quaternion() * AdditionalRotation.Quaternion()).Rotator();
}
