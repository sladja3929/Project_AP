#include "GAS/Abilities/Enemy/EnemyProjectileAbility.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Projectiles/EnemyProjectile.h"
#include "AI/EnemyAIController.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyProjectileAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyProjectileAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UEnemyProjectileAbility::UEnemyProjectileAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

#pragma region "Activate Initialization"

void UEnemyProjectileAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	CacheProjectileConfig();
	CacheProjectileTags();
}

void UEnemyProjectileAbility::CacheProjectileConfig()
{
	if (EnemyAttackData && EnemyAttackData->AttackType == EComboAttackType::Projectile)
	{
		CachedProjectileConfig = &EnemyAttackData->ProjectileConfig;
	}
	else
	{
		CachedProjectileConfig = nullptr;
		DEBUG_LOG(TEXT("CacheProjectileConfig: AttackType is not Projectile"));
	}
}

void UEnemyProjectileAbility::CacheProjectileTags()
{
	EventNotifyProjectileSpawnTag = UGameplayTagsSubsystem::GetEventNotifyProjectileSpawnTag();
	EventNotifyProjectileGrabTag = UGameplayTagsSubsystem::GetEventNotifyProjectileGrabTag();
	EventNotifyProjectileLaunchTag = UGameplayTagsSubsystem::GetEventNotifyProjectileLaunchTag();
}

#pragma endregion

#pragma region "Ability Lifecycle"

void UEnemyProjectileAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//부모의 ActivateAbility → ActivateInitSettings → ExecuteAttack(몽타주 시작) 호출
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	if (!CachedProjectileConfig || !CachedProjectileConfig->ProjectileClass)
	{
		DEBUG_LOG(TEXT("ActivateAbility: No projectile config or class — ending ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//몽타주 노티파이 이벤트 대기 태스크 등록
	START_WAIT_EVENT_TASK(WaitProjectileSpawnEventTask, EventNotifyProjectileSpawnTag, OnEventProjectileSpawn, nullptr, true, true);
	START_WAIT_EVENT_TASK(WaitProjectileGrabEventTask, EventNotifyProjectileGrabTag, OnEventProjectileGrab, nullptr, true, true);
	START_WAIT_EVENT_TASK(WaitProjectileLaunchEventTask, EventNotifyProjectileLaunchTag, OnEventProjectileLaunch, nullptr, true, true);
}

#pragma endregion

#pragma region "Hit Detection"

void UEnemyProjectileAbility::SetHitDetectionConfig()
{
	//투사체 콤보는 본체(EnemyAttackComponent) 판정을 준비하지 않음 — 판정·데미지는 투사체가 담당
	if (ComboCounter == 0)
	{
		return;
	}

	Super::SetHitDetectionConfig();
}

#pragma endregion

#pragma region "Projectile"

void UEnemyProjectileAbility::SpawnProjectile()
{
	AEnemyCharacter* EnemyCharacter = GetEnemyCharacterFromActorInfo();
	UWorld* World = GetWorld();

	if (!CachedProjectileConfig || !CachedProjectileConfig->ProjectileClass || !EnemyCharacter || !World || !EnemyCharacter->HasAuthority())
	{
		return;
	}

	//같은 몽타주에서 중복 수신 시 이전 투사체 정리
	DestroyPendingProjectile();

	//등장 위치(오너 로컬 오프셋) 또는 손 소켓 위치에 생성
	FTransform SpawnTransform = EnemyCharacter->GetActorTransform();

	if (CachedProjectileConfig->bPresentBeforeGrab)
	{
		SpawnTransform.SetLocation(EnemyCharacter->GetActorTransform().TransformPosition(CachedProjectileConfig->PresentOffset));
	}
	else if (const USkeletalMeshComponent* Mesh = EnemyCharacter->GetMesh())
	{
		SpawnTransform = Mesh->GetSocketTransform(CachedProjectileConfig->AttachSocketName);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = EnemyCharacter;
	SpawnParams.Instigator = EnemyCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(CachedProjectileConfig->ProjectileClass, SpawnTransform.GetLocation(), SpawnTransform.Rotator(), SpawnParams);

	if (!Projectile)
	{
		DEBUG_LOG(TEXT("SpawnProjectile: Spawn failed"));
		return;
	}

	Projectile->InitProjectile(GetEnemyAbilitySystemComponentFromActorInfo(), DamageInstantEffect, GetAbilityLevel(), GetAssetTags(), ComboCounter);
	PendingProjectile = Projectile;

	if (CachedProjectileConfig->bPresentBeforeGrab)
	{
		Projectile->PresentAt(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
	}
	else
	{
		GrabProjectile();
	}

	DEBUG_LOG(TEXT("SpawnProjectile: %s (Present=%d)"), *Projectile->GetName(), CachedProjectileConfig->bPresentBeforeGrab);
}

void UEnemyProjectileAbility::GrabProjectile()
{
	AEnemyCharacter* EnemyCharacter = GetEnemyCharacterFromActorInfo();

	if (!PendingProjectile || !CachedProjectileConfig || !EnemyCharacter)
	{
		DEBUG_LOG(TEXT("GrabProjectile: No pending projectile"));
		return;
	}

	PendingProjectile->AttachToSocket(EnemyCharacter->GetMesh(), CachedProjectileConfig->AttachSocketName, CachedProjectileConfig->SecondaryAttachSocketName);
}

void UEnemyProjectileAbility::LaunchProjectile()
{
	if (!PendingProjectile)
	{
		DEBUG_LOG(TEXT("LaunchProjectile: No pending projectile"));
		return;
	}

	AEnemyProjectile* Projectile = PendingProjectile;
	PendingProjectile = nullptr;

	FVector LaunchVelocity;

	if (!CalculateLaunchVelocity(Projectile->GetActorLocation(), Projectile->GetProjectileGravityZ(), LaunchVelocity))
	{
		//타깃이 없으면 정면 수평 발사
		const AEnemyCharacter* EnemyCharacter = GetEnemyCharacterFromActorInfo();
		const FVector Forward = EnemyCharacter ? EnemyCharacter->GetActorForwardVector() : FVector::ForwardVector;
		LaunchVelocity = Forward * (CachedProjectileConfig ? CachedProjectileConfig->HorizontalSpeed : 2000.0f);
	}

	Projectile->Launch(LaunchVelocity);
}

void UEnemyProjectileAbility::DestroyPendingProjectile()
{
	if (PendingProjectile)
	{
		PendingProjectile->Destroy();
		PendingProjectile = nullptr;
	}
}

bool UEnemyProjectileAbility::CalculateLaunchVelocity(const FVector& StartLocation, float GravityZ, FVector& OutVelocity) const
{
	if (!CachedProjectileConfig)
	{
		return false;
	}

	//발사 시점 최신 타깃 (없으면 어빌리티 시작 시 캐싱한 타깃)
	const AActor* TargetActor = nullptr;

	if (const AEnemyAIController* AIController = GetEnemyAIControllerFromActorInfo())
	{
		TargetActor = AIController->GetCurrentTarget().Actor.Get();
	}

	if (!TargetActor)
	{
		TargetActor = CachedTargetInfo.Actor.Get();
	}

	if (!TargetActor)
	{
		return false;
	}

	const FEnemyProjectileConfig& Config = *CachedProjectileConfig;
	const FVector TargetLocation = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, Config.TargetHeightOffset);
	const FVector TargetVelocity2D(TargetActor->GetVelocity().X, TargetActor->GetVelocity().Y, 0.0f);
	const float MaxFlightTime = FMath::Max(Config.MinFlightTime, Config.MaxFlightTime);

	auto GetFlightTime = [&](const FVector& AimLocation)
	{
		return FMath::Clamp(FVector::Dist2D(StartLocation, AimLocation) / Config.HorizontalSpeed, Config.MinFlightTime, MaxFlightTime);
	};

	//예측 지점까지 거리가 달라지므로 비행 시간을 한 번 더 맞춤
	float FlightTime = GetFlightTime(TargetLocation);
	FVector AimLocation = TargetLocation + TargetVelocity2D * FlightTime * Config.LeadFactor;
	FlightTime = GetFlightTime(AimLocation);
	AimLocation = TargetLocation + TargetVelocity2D * FlightTime * Config.LeadFactor;

	//P(T) = Start + V·T + ½·g·T² → V = (Aim - Start) / T - ½·g·T
	OutVelocity = (AimLocation - StartLocation) / FlightTime - FVector(0.0f, 0.0f, 0.5f * GravityZ * FlightTime);

	DEBUG_LOG(TEXT("CalculateLaunchVelocity: Target=%s, FlightTime=%.2f, Velocity=%s"), *GetNameSafe(TargetActor), FlightTime, *OutVelocity.ToString());
	return true;
}

#pragma endregion

#pragma region "Event Handlers"

void UEnemyProjectileAbility::OnEventProjectileSpawn(FGameplayEventData Payload)
{
	SpawnProjectile();
}

void UEnemyProjectileAbility::OnEventProjectileGrab(FGameplayEventData Payload)
{
	GrabProjectile();
}

void UEnemyProjectileAbility::OnEventProjectileLaunch(FGameplayEventData Payload)
{
	LaunchProjectile();
}

#pragma endregion

void UEnemyProjectileAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//발사 전에 끝나면(피격·그로기 취소 등) 등장해 있거나 들고 있던 투사체 제거. 발사된 투사체는 독립적으로 판정 유지
	DestroyPendingProjectile();

	END_ABILITY_TASK(WaitProjectileSpawnEventTask);
	END_ABILITY_TASK(WaitProjectileGrabEventTask);
	END_ABILITY_TASK(WaitProjectileLaunchEventTask);

	CachedProjectileConfig = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
