#include "Characters/HitDetection/ProjectileHitDetectionComponent.h"
#include "Projectiles/BaseProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogProjectileHitDetectionComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogProjectileHitDetectionComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UProjectileHitDetectionComponent::UProjectileHitDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	//ProjectileMovement(PrePhysics)가 이번 프레임 이동을 끝낸 뒤 이동 구간 판정
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UProjectileHitDetectionComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerProjectile = Cast<ABaseProjectile>(GetOwner());

	if (!OwnerProjectile)
	{
		DEBUG_LOG(TEXT("BeginPlay: Owner is not a BaseProjectile"));
	}
}

void UProjectileHitDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SweepToCurrentLocation();
}

#pragma region "HitDetection Interface"

void UProjectileHitDetectionComponent::PrepareHitDetection(const FGameplayTagContainer& AttackTags, const int32 ComboIndex)
{
	//BeginPlay 전(지연 스폰 직후)에도 호출될 수 있어 여기서도 소유자 확인
	if (!OwnerProjectile)
	{
		OwnerProjectile = Cast<ABaseProjectile>(GetOwner());
	}

	bIsPrepared = OwnerProjectile && OwnerProjectile->ResolveAttackData(AttackTags, ComboIndex, CurrentAttackData);
	HitActors.Empty();

	DEBUG_LOG(TEXT("PrepareHitDetection: Tags=%s, Combo=%d, Prepared=%d, Damage=%.1f"),
		*AttackTags.ToStringSimple(), ComboIndex, bIsPrepared, CurrentAttackData.FinalDamage);
}

void UProjectileHitDetectionComponent::HandleHitDetectionStart(const FGameplayEventData& Payload)
{
	if (!bIsPrepared || !GetOwner() || !GetOwner()->HasAuthority())
	{
		DEBUG_LOG(TEXT("HandleHitDetectionStart: Ignored (Prepared=%d)"), bIsPrepared);
		return;
	}

	LastTraceLocation = GetOwner()->GetActorLocation();
	bIsDetecting = true;
	SetComponentTickEnabled(true);
}

void UProjectileHitDetectionComponent::HandleHitDetectionEnd(const FGameplayEventData& Payload)
{
	bIsDetecting = false;
	SetComponentTickEnabled(false);
}

#pragma endregion

#pragma region "Trace Functions"

void UProjectileHitDetectionComponent::SweepToCurrentLocation()
{
	if (!bIsDetecting || !GetOwner() || !GetWorld())
	{
		return;
	}

	const FVector Start = LastTraceLocation;
	const FVector End = GetOwner()->GetActorLocation();
	LastTraceLocation = End;

	//이동이 없는 프레임은 길이 0 스윕 = 현재 위치 오버랩 판정 (AttackTraceComponent와 같은 규칙)
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_GameTraceChannel1, FCollisionShape::MakeSphere(TraceRadius), GetCollisionQueryParams());

	//시작부터 겹친 히트는 ImpactPoint가 부정확할 수 있어 현재 위치로 보정 (임팩트 GameplayCue 위치로 사용됨)
	for (FHitResult& Hit : Hits)
	{
		if (Hit.bStartPenetrating)
		{
			Hit.ImpactPoint = End;
		}
	}

	if (bDrawDebugTrace)
	{
		DrawDebugSphere(GetWorld(), End, TraceRadius, 12, FColor::Orange, false, DebugTraceDuration);
		DrawDebugLine(GetWorld(), Start, End, FColor::Orange, false, DebugTraceDuration);
	}

	ProcessHits(Hits);
}

void UProjectileHitDetectionComponent::DetectRadius(const FVector& Center, float Radius)
{
	if (!bIsPrepared || Radius <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return;
	}

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, Center, Center, FQuat::Identity, ECC_GameTraceChannel1, FCollisionShape::MakeSphere(Radius), GetCollisionQueryParams());

	//길이 0 스윕 히트는 ImpactPoint가 반경 중심이라 맞은 바디(본) 위치로 보정 (피격 이펙트가 대상 몸에서 재생되도록)
	for (FHitResult& Hit : Hits)
	{
		const USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Hit.GetComponent());

		if (SkeletalMesh && Hit.BoneName != NAME_None)
		{
			Hit.ImpactPoint = SkeletalMesh->GetBoneLocation(Hit.BoneName);
		}
		else if (Hit.GetActor())
		{
			Hit.ImpactPoint = Hit.GetActor()->GetActorLocation();
		}

		Hit.ImpactNormal = (Hit.ImpactPoint - Center).GetSafeNormal();
	}

	if (bDrawDebugTrace)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 16, FColor::Red, false, DebugTraceDuration);
	}

	ProcessHits(Hits);
}

void UProjectileHitDetectionComponent::ProcessHits(const TArray<FHitResult>& Hits)
{
	//비행 중 스윕에서 호출됐는지 (착탄 반경 판정은 판정 종료 후 호출되므로 false)
	const bool bWasDetecting = bIsDetecting;

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();

		if (!ValidateHit(HitActor))
		{
			continue;
		}

		HitActors.Add(HitActor);

		DEBUG_LOG(TEXT("ProcessHits: Hit %s (Bone=%s)"), *GetNameSafe(HitActor), *Hit.BoneName.ToString());
		OnHit.Broadcast(HitActor, Hit, CurrentAttackData);

		//브로드캐스트 안에서 착탄 처리로 비행 판정이 끝났으면 남은 히트는 버림 (직격 시 정지 투사체)
		if (bWasDetecting && !bIsDetecting)
		{
			return;
		}
	}
}

bool UProjectileHitDetectionComponent::ValidateHit(const AActor* HitActor) const
{
	if (!HitActor || !OwnerProjectile)
	{
		return false;
	}

	if (HitActor == OwnerProjectile || HitActor == OwnerProjectile->GetInstigator())
	{
		return false;
	}

	if (OwnerProjectile->IsFriendlyActor(HitActor))
	{
		return false;
	}

	return !HitActors.Contains(HitActor);
}

FCollisionQueryParams UProjectileHitDetectionComponent::GetCollisionQueryParams() const
{
	FCollisionQueryParams Params(TEXT("ProjectileHitDetection"), false);
	Params.bReturnPhysicalMaterial = true;
	Params.AddIgnoredActor(GetOwner());

	if (OwnerProjectile && OwnerProjectile->GetInstigator())
	{
		Params.AddIgnoredActor(OwnerProjectile->GetInstigator());
	}

	return Params;
}

#pragma endregion
