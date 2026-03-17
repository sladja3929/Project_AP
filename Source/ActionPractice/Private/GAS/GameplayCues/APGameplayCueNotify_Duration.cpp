#include "GAS/GameplayCues/APGameplayCueNotify_Duration.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAPGameplayCueNotify_Duration, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAPGameplayCueNotify_Duration, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Constructor"

AAPGameplayCueNotify_Duration::AAPGameplayCueNotify_Duration()
{
	//Cue Actor는 코스메틱 전용이므로 리플리케이트하지 않는다.
	bAutoDestroyOnRemove = true;
}

#pragma endregion

#pragma region "GameplayCueNotify_Actor Overrides"

bool AAPGameplayCueNotify_Duration::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	DEBUG_LOG(TEXT("OnActive - Target: %s"), MyTarget ? *MyTarget->GetName() : TEXT("NULL"));

	if (!MyTarget)
	{
		return false;
	}

	//Niagara 이펙트 스폰
	SpawnNiagaraEffect(MyTarget);

	//루핑 사운드 스폰
	SpawnLoopingSound(MyTarget);

	//적용 시 원샷 사운드 재생
	PlayOneShotSound(MyTarget, OneShotActivateSound);

	return false;
}

bool AAPGameplayCueNotify_Duration::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	DEBUG_LOG(TEXT("WhileActive - Target: %s, NiagaraAlreadySpawned: %s"),
		MyTarget ? *MyTarget->GetName() : TEXT("NULL"),
		SpawnedNiagara ? TEXT("True") : TEXT("False"));

	//이미 스폰된 경우 중복 방지 (레벨 전환 복원 등)
	if (!SpawnedNiagara && MyTarget)
	{
		SpawnNiagaraEffect(MyTarget);
	}

	if (!SpawnedAudio && MyTarget && LoopingSound)
	{
		SpawnLoopingSound(MyTarget);
	}

	return false;
}

bool AAPGameplayCueNotify_Duration::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	DEBUG_LOG(TEXT("OnRemove - Target: %s"), MyTarget ? *MyTarget->GetName() : TEXT("NULL"));

	//제거 직전 콜백
	if (SpawnedNiagara)
	{
		OnEffectRemoved(SpawnedNiagara);
		SpawnedNiagara->Deactivate();
		SpawnedNiagara->DestroyComponent();
		SpawnedNiagara = nullptr;
	}

	if (SpawnedAudio)
	{
		SpawnedAudio->Stop();
		SpawnedAudio->DestroyComponent();
		SpawnedAudio = nullptr;
	}

	//제거 시 원샷 사운드 재생
	PlayOneShotSound(MyTarget, OneShotRemoveSound);

	return false;
}

#pragma endregion

#pragma region "Extension Points"

USceneComponent* AAPGameplayCueNotify_Duration::GetAttachTarget_Implementation(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return nullptr;
	}

	//SkeletalMeshComponent를 우선 탐색
	USkeletalMeshComponent* MeshComp = TargetActor->FindComponentByClass<USkeletalMeshComponent>();
	if (MeshComp)
	{
		return MeshComp;
	}

	//없으면 루트 컴포넌트 반환
	return TargetActor->GetRootComponent();
}

void AAPGameplayCueNotify_Duration::OnEffectSpawned_Implementation(UNiagaraComponent* NiagaraComp)
{
	//기본 구현: 없음
	//서브클래스에서 Niagara 파라미터 주입, 머티리얼 동적 변경 등에 사용
}

void AAPGameplayCueNotify_Duration::OnEffectRemoved_Implementation(UNiagaraComponent* NiagaraComp)
{
	//기본 구현: 없음
	//서브클래스에서 페이드아웃 등 정리 로직에 사용
}

#pragma endregion

#pragma region "Private Functions"

void AAPGameplayCueNotify_Duration::SpawnNiagaraEffect(AActor* TargetActor)
{
	if (!NiagaraEffect || !TargetActor)
	{
		return;
	}

	USceneComponent* AttachTarget = GetAttachTarget(TargetActor);
	if (!AttachTarget)
	{
		DEBUG_LOG(TEXT("SpawnNiagaraEffect - AttachTarget is NULL"));
		return;
	}

	if (bAttachToSocket)
	{
		SpawnedNiagara = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraEffect,
			AttachTarget,
			AttachSocketName,
			LocationOffset,
			RotationOffset,
			EffectScale,
			EAttachLocation::KeepRelativeOffset,
			true,
			ENCPoolMethod::None
		);
	}
	else
	{
		SpawnedNiagara = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraEffect,
			AttachTarget,
			NAME_None,
			LocationOffset,
			RotationOffset,
			EffectScale,
			EAttachLocation::KeepRelativeOffset,
			true,
			ENCPoolMethod::None
		);
	}

	if (SpawnedNiagara)
	{
		OnEffectSpawned(SpawnedNiagara);
		DEBUG_LOG(TEXT("SpawnNiagaraEffect - Spawned: %s on Socket: %s"),
			*NiagaraEffect->GetName(),
			bAttachToSocket ? *AttachSocketName.ToString() : TEXT("Root"));
	}
}

void AAPGameplayCueNotify_Duration::SpawnLoopingSound(AActor* TargetActor)
{
	if (!LoopingSound || !TargetActor)
	{
		return;
	}

	USceneComponent* AttachTarget = GetAttachTarget(TargetActor);
	if (!AttachTarget)
	{
		return;
	}

	if (bAttachToSocket)
	{
		SpawnedAudio = UGameplayStatics::SpawnSoundAttached(
			LoopingSound,
			AttachTarget,
			AttachSocketName,
			LocationOffset,
			RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			true
		);
	}
	else
	{
		SpawnedAudio = UGameplayStatics::SpawnSoundAttached(
			LoopingSound,
			AttachTarget,
			NAME_None,
			LocationOffset,
			RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			true
		);
	}

	if (SpawnedAudio)
	{
		DEBUG_LOG(TEXT("SpawnLoopingSound - Spawned: %s"), *LoopingSound->GetName());
	}
}

void AAPGameplayCueNotify_Duration::PlayOneShotSound(AActor* TargetActor, USoundBase* Sound)
{
	if (!Sound || !TargetActor)
	{
		return;
	}

	USceneComponent* AttachTarget = GetAttachTarget(TargetActor);
	if (AttachTarget && bAttachToSocket)
	{
		UGameplayStatics::SpawnSoundAttached(
			Sound,
			AttachTarget,
			AttachSocketName,
			LocationOffset,
			RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			true
		);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			TargetActor,
			Sound,
			TargetActor->GetActorLocation()
		);
	}
}

#pragma endregion
