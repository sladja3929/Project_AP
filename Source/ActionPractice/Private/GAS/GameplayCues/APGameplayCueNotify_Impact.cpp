#include "GAS/GameplayCues/APGameplayCueNotify_Impact.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/Effects/ActionPracticeGameplayEffectContext.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAPGameplayCueNotify_Impact, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAPGameplayCueNotify_Impact, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Constructor"

UAPGameplayCueNotify_Impact::UAPGameplayCueNotify_Impact()
{
	//Impact 큐는 기본적으로 소켓이 아닌 피격 위치에 스폰
	bAttachToSocket = false;
}

#pragma endregion

#pragma region "GameplayCueNotify_Static Overrides"

bool UAPGameplayCueNotify_Impact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	DEBUG_LOG(TEXT("OnExecute - Target: %s"), MyTarget ? *MyTarget->GetName() : TEXT("NULL"));

	if (!MyTarget)
	{
		return false;
	}

	//속성 기반 이펙트 분기 (기본 구현은 멤버 프로퍼티를 그대로 반환)
	UNiagaraSystem* ResolvedNiagara = nullptr;
	USoundBase* ResolvedSound = nullptr;
	ResolveEffectByContext(Parameters, ResolvedNiagara, ResolvedSound);

	//ResolveEffectByContext가 nullptr을 반환하면 멤버 프로퍼티 폴백
	UNiagaraSystem* FinalNiagara = ResolvedNiagara ? ResolvedNiagara : NiagaraEffect.Get();
	USoundBase* FinalSound = ResolvedSound ? ResolvedSound : InstantSound.Get();

	FVector SpawnLocation;
	FRotator SpawnRotation;
	GetSpawnTransform(MyTarget, Parameters, SpawnLocation, SpawnRotation);

	//Niagara 원샷 스폰
	if (FinalNiagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MyTarget->GetWorld(),
			FinalNiagara,
			SpawnLocation,
			SpawnRotation,
			EffectScale,
			true,
			true,
			ENCPoolMethod::None
		);

		DEBUG_LOG(TEXT("OnExecute - Impact Niagara Spawned: %s at %s, Rotation: %s"),
			*FinalNiagara->GetName(),
			*SpawnLocation.ToString(),
			*SpawnRotation.ToString());
	}

	//사운드 원샷 재생
	if (FinalSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			MyTarget,
			FinalSound,
			SpawnLocation
		);
	}

	return false;
}

#pragma endregion

#pragma region "Extension Points"

void UAPGameplayCueNotify_Impact::GetSpawnTransform_Implementation(AActor* TargetActor, const FGameplayCueParameters& Parameters, FVector& OutLocation, FRotator& OutRotation) const
{
	//Parameters에 유효한 Location이 있으면 피격 위치 사용
	if (!Parameters.Location.IsNearlyZero())
	{
		OutLocation = Parameters.Location;
	}
	else if (TargetActor)
	{
		//폴백: 타겟 액터 위치
		OutLocation = TargetActor->GetActorLocation();
	}
	else
	{
		OutLocation = FVector::ZeroVector;
	}

	//방향 결정
	if (bAlignToNormal && !Parameters.Normal.IsNearlyZero())
	{
		OutRotation = Parameters.Normal.Rotation() + RotationOffset;
	}
	else if (TargetActor)
	{
		OutRotation = TargetActor->GetActorRotation() + RotationOffset;
	}
	else
	{
		OutRotation = RotationOffset;
	}
}

void UAPGameplayCueNotify_Impact::ResolveEffectByContext_Implementation(const FGameplayCueParameters& Parameters, UNiagaraSystem*& OutNiagara, USoundBase*& OutSound) const
{
	OutNiagara = nullptr;
	OutSound = nullptr;

	if (!ImpactResponseDA)
	{
		//DA가 없으면 nullptr 반환 → OnExecute에서 멤버 프로퍼티 폴백
		return;
	}

	// ===== 가드 여부 확인 =====
	bool bIsBlocked = false;
	AActor* TargetActor = Parameters.TargetAttachComponent.IsValid()
		? Parameters.TargetAttachComponent->GetOwner()
		: nullptr;

	//Parameters에서 타겟을 못 가져오면 EffectContext에서 시도
	if (!TargetActor)
	{
		const FGameplayEffectContext* Context = Parameters.EffectContext.Get();
		if (Context)
		{
			const FHitResult* HitResult = Context->GetHitResult();
			if (HitResult)
			{
				TargetActor = HitResult->GetActor();
			}
		}
	}

	if (TargetActor)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (TargetASC && TargetASC->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateAbilityBlockingTag()))
		{
			bIsBlocked = true;
		}
	}

	// ===== 표면 재질 확인 =====
	EPhysicalSurface SurfaceType = EPhysicalSurface::SurfaceType_Default;

	//GAS는 HitResult의 PhysMaterial을 Parameters.PhysicalMaterial에 자동 매핑하지 않으므로,
	//EffectContext의 HitResult에서 직접 읽는다.
	const FGameplayEffectContext* Context = Parameters.EffectContext.Get();
	if (Context)
	{
		const FHitResult* HitResult = Context->GetHitResult();
		if (HitResult && HitResult->PhysMaterial.IsValid())
		{
			SurfaceType = HitResult->PhysMaterial->SurfaceType;
		}
	}

	// ===== DA 룩업 =====
	const FImpactResponseData& ResponseData = ImpactResponseDA->GetResponse(SurfaceType, bIsBlocked);

	OutNiagara = ResponseData.NiagaraEffect.Get();
	OutSound = ResponseData.Sound.Get();

	DEBUG_LOG(TEXT("ResolveEffectByContext - SurfaceType: %d, Blocked: %s, Niagara: %s, Sound: %s"),
		static_cast<int32>(SurfaceType),
		bIsBlocked ? TEXT("True") : TEXT("False"),
		OutNiagara ? *OutNiagara->GetName() : TEXT("NULL (fallback)"),
		OutSound ? *OutSound->GetName() : TEXT("NULL (fallback)"));
}

#pragma endregion
