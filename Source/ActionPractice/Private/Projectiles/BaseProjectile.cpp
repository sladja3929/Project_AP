#include "Projectiles/BaseProjectile.h"
#include "Characters/HitDetection/ProjectileHitDetectionComponent.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBaseProjectile, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseProjectile, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ABaseProjectile::ABaseProjectile()
{
	//양손 들기 중에만 틱 (메시 애니메이션 갱신 후 소켓 위치를 읽도록 PostPhysics)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	bReplicates = true;
	SetReplicatingMovement(true);

	//이동 충돌: 월드 지형·구조물만 Block. 캐릭터 캡슐(Pawn)·히트 바디(WeaponTrace)는 히트 판정 컴포넌트가 별도 스윕
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(20.0f);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCanEverAffectNavigation(false);
	RootComponent = CollisionComponent;

	MeshPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MeshPivot"));
	MeshPivot->SetupAttachment(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(MeshPivot);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCanEverAffectNavigation(false);

	//발사 전(소켓 부착 중)에는 비활성. 활성 상태를 복제해 클라이언트도 같은 탄도로 시뮬레이션
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->SetIsReplicated(true);
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;

	HitDetectionComponent = CreateDefaultSubobject<UProjectileHitDetectionComponent>(TEXT("HitDetectionComponent"));
}

void ABaseProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseProjectile, Phase);
	DOREPLIFETIME(ABaseProjectile, HoldSecondarySocketName);
}

void ABaseProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const USceneComponent* Parent = RootComponent ? RootComponent->GetAttachParent() : nullptr;

	if (Phase != EProjectilePhase::Held || HoldSecondarySocketName.IsNone() || !Parent)
	{
		return;
	}

	//부착 소켓과 두 번째 소켓의 중간 (서버·클라이언트가 같은 메시 포즈로 각자 계산)
	const FVector PrimaryLocation = Parent->GetSocketLocation(RootComponent->GetAttachSocketName());
	const FVector SecondaryLocation = Parent->GetSocketLocation(HoldSecondarySocketName);
	SetActorLocation((PrimaryLocation + SecondaryLocation) * 0.5f);
}

void ABaseProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);

	//복제된 속도를 로컬 시뮬레이션에 반영 (위치 보정 사이 구간을 같은 탄도로 이어감)
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = NewVelocity;
	}
}

#pragma region "Launch"

void ABaseProjectile::InitProjectile(UBaseAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageEffect, float InEffectLevel, const FGameplayTagContainer& InAttackTags, int32 InComboIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceASC = InSourceASC;
	DamageEffect = InDamageEffect;
	EffectLevel = InEffectLevel;
	AttackTags = InAttackTags;
	ComboIndex = InComboIndex;

	//히트 판정 전략 연결 후 공격 데이터 주입
	HitDetectionSetter.Init(HitDetectionComponent.Get());
	HitDetectionSetter.Bind(this);
	SetHitDetectionConfig();
}

void ABaseProjectile::PresentAt(const FVector& Location, const FRotator& Rotation)
{
	if (!HasAuthority() || Phase >= EProjectilePhase::Launched)
	{
		return;
	}

	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(Location, Rotation);
	SetPhase(EProjectilePhase::Presented);
}

void ABaseProjectile::AttachToSocket(USceneComponent* Parent, FName SocketName, FName SecondarySocketName)
{
	if (!HasAuthority() || !Parent || Phase >= EProjectilePhase::Launched)
	{
		return;
	}

	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//위치만 소켓에 맞추고 회전은 놓여 있던 각도 유지 (잡는 순간 튀지 않고 이후 손 회전을 따라감)
	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	AttachToComponent(Parent, AttachRules, SocketName);
	HoldSecondarySocketName = SecondarySocketName;
	SetPhase(EProjectilePhase::Held);
}

void ABaseProjectile::Launch(const FVector& LaunchVelocity)
{
	if (!HasAuthority() || Phase >= EProjectilePhase::Launched)
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	EnableFlightCollision();

	ProjectileMovement->OnProjectileStop.AddUniqueDynamic(this, &ABaseProjectile::HandleProjectileStop);
	ProjectileMovement->Velocity = LaunchVelocity;
	ProjectileMovement->Activate(true);

	SetLifeSpan(MaxFlightTime);

	SetPhase(EProjectilePhase::Launched);

	//비행 중 히트 판정 시작 (ANS 이벤트가 아닌 발사 시점 직접 호출)
	HitDetectionComponent->HandleHitDetectionStart(FGameplayEventData());

	DEBUG_LOG(TEXT("Launch: %s Velocity=%s"), *GetName(), *LaunchVelocity.ToString());
}

float ABaseProjectile::GetProjectileGravityZ() const
{
	return ProjectileMovement ? ProjectileMovement->GetGravityZ() : 0.0f;
}

#pragma endregion

#pragma region "Hit Detection"

void ABaseProjectile::SetHitDetectionConfig()
{
	if (!HitDetectionSetter.PrepareHitDetection(AttackTags, ComboIndex))
	{
		DEBUG_LOG(TEXT("SetHitDetectionConfig: Failed to prepare hit detection (%s)"), *GetName());
	}
}

void ABaseProjectile::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
	UBaseAbilitySystemComponent* SourceAbilitySystem = SourceASC.Get();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	bool bDamageApplied = false;

	//회피 무적 대상은 통과 (대미지 GE의 모디파이어 태그 조건으로 막히며, GE 적용 자체는 성공으로 반환되므로 직접 확인)
	if (TargetASC && TargetASC->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateInvincibleTag()))
	{
		DEBUG_LOG(TEXT("OnHitDetected: %s is invincible, pass through"), *GetNameSafe(HitActor));
		return;
	}

	//공격자 ASC로 공격 GE 생성 (근접 공격과 같은 데미지 파이프라인, SourceObject는 투사체)
	if (SourceAbilitySystem && TargetASC && DamageEffect)
	{
		FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->CreateAttackGameplayEffectSpec(DamageEffect, EffectLevel, this, AttackData, &HitResult);

		if (SpecHandle.IsValid())
		{
			//GE 적용 조건(면역·적용 태그 요구)에 막히면 false
			bDamageApplied = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC).WasSuccessfullyApplied();
			DEBUG_LOG(TEXT("OnHitDetected: %s damage %.1f to %s"), bDamageApplied ? TEXT("Applied") : TEXT("Blocked"), AttackData.FinalDamage, *GetNameSafe(HitActor));
		}
	}

	//GE 적용이 막힌 대상은 통과
	if (bStopOnCharacterHit && bDamageApplied && Phase != EProjectilePhase::Impacted)
	{
		Impact(HitResult);
	}
}

#pragma endregion

#pragma region "Impact"

void ABaseProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || Phase != EProjectilePhase::Launched)
	{
		return;
	}

	//멈추기 직전 프레임 이동 구간의 캐릭터 판정을 먼저 처리 (직격이면 그 안에서 착탄 처리됨)
	HitDetectionComponent->SweepToCurrentLocation();

	if (Phase == EProjectilePhase::Launched)
	{
		Impact(ImpactResult);
	}
}

void ABaseProjectile::Impact(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || Phase != EProjectilePhase::Launched)
	{
		return;
	}

	SetPhase(EProjectilePhase::Impacted);

	const FVector ImpactLocation = ImpactResult.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(ImpactResult.ImpactPoint);
	const FVector ImpactNormal = ImpactResult.ImpactNormal.IsNearlyZero() ? FVector::UpVector : FVector(ImpactResult.ImpactNormal);

	//비행 판정을 먼저 끝낸 뒤 반경 판정 (ProcessHits가 비행 중 호출 여부로 직격 정지와 반경 판정을 구분하므로 순서 유지)
	HitDetectionComponent->HandleHitDetectionEnd(FGameplayEventData());
	HitDetectionComponent->DetectRadius(ImpactLocation, ImpactRadius);

	MulticastPlayImpactEffect(ImpactLocation, ImpactNormal);

	//바로 Destroy하면 착탄 위치 복제·멀티캐스트보다 액터 제거가 먼저 도착할 수 있어 숨김 후 제거 예약
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(1.0f);

	DEBUG_LOG(TEXT("Impact: %s at %s"), *GetName(), *ImpactLocation.ToString());
}

void ABaseProjectile::MulticastPlayImpactEffect_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal)
{
	//클라이언트는 서버 착탄 전에 로컬 시뮬레이션이 먼저 멈췄을 수 있으므로 서버 착탄 위치 기준으로 숨김
	SetActorHiddenInGame(true);

	UEffectManagerSubsystem* EffectManager = UEffectManagerSubsystem::Get(this);

	if (!EffectManager)
	{
		return;
	}

	FEnvironmentImpactRequest Request = ImpactEffect;
	Request.Location = Location;
	Request.Normal = Normal;
	EffectManager->SpawnEnvironmentImpact(Request);

	for (const FEnvironmentImpactRequest& Additional : AdditionalImpactEffects)
	{
		FEnvironmentImpactRequest AdditionalRequest = Additional;
		AdditionalRequest.Location = Location;
		AdditionalRequest.Normal = Normal;
		EffectManager->SpawnEnvironmentImpact(AdditionalRequest);
	}
}

#pragma endregion

#pragma region "Phase"

void ABaseProjectile::SetPhase(EProjectilePhase NewPhase)
{
	if (!HasAuthority() || Phase == NewPhase)
	{
		return;
	}

	const EProjectilePhase OldPhase = Phase;
	Phase = NewPhase;

	RefreshHoldTick();

	//서버는 OnRep이 호출되지 않으므로 직접 호출 (데디 서버면 내부에서 생략)
	NotifyPhaseChanged(NewPhase, OldPhase);
}

void ABaseProjectile::OnRep_Phase(EProjectilePhase OldPhase)
{
	//클라이언트 ProjectileMovement 시뮬레이션도 지형에 막히도록 (서버 착탄 멀티캐스트 도착 전 지형 통과 방지)
	if (Phase == EProjectilePhase::Launched)
	{
		EnableFlightCollision();
	}

	RefreshHoldTick();
	NotifyPhaseChanged(Phase, OldPhase);
}

void ABaseProjectile::EnableFlightCollision()
{
	//발사자와는 겹친 채 출발하므로 이동 무시
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (AActor* InstigatorActor = GetInstigator())
	{
		CollisionComponent->IgnoreActorWhenMoving(InstigatorActor, true);
	}
}

void ABaseProjectile::OnRep_HoldSecondarySocketName()
{
	RefreshHoldTick();
}

void ABaseProjectile::RefreshHoldTick()
{
	SetActorTickEnabled(Phase == EProjectilePhase::Held && !HoldSecondarySocketName.IsNone());
}

void ABaseProjectile::NotifyPhaseChanged(EProjectilePhase NewPhase, EProjectilePhase OldPhase)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	K2_OnProjectilePhaseChanged(NewPhase, OldPhase);
}

#pragma endregion

void ABaseProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HitDetectionSetter.UnBind();

	Super::EndPlay(EndPlayReason);
}
