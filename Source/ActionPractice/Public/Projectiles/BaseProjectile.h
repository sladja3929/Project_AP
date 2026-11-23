#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Effects/EffectManagerSubsystem.h"
#include "GAS/Abilities/HitDetectionSetter.h"
#include "BaseProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UProjectileHitDetectionComponent;
class UBaseAbilitySystemComponent;
class UGameplayEffect;

//투사체 진행 단계 (서버가 바꾸고 복제, 바뀔 때마다 BP 연출 이벤트 호출)
UENUM(BlueprintType)
enum class EProjectilePhase : uint8
{
	None UMETA(DisplayName = "None"),
	//던지기 전 월드에 놓여 있음 (등장 연출)
	Presented UMETA(DisplayName = "Presented"),
	//소켓에 붙어 들려 있음
	Held UMETA(DisplayName = "Held"),
	//비행 중
	Launched UMETA(DisplayName = "Launched"),
	//착탄 (숨김 후 제거 대기)
	Impacted UMETA(DisplayName = "Impacted"),
};

/**
 * 플레이어/적 공용 투사체
 *
 * -생성 후 InitProjectile로 데미지 GE 출처(공격자 ASC)와 공격 식별 정보를 받고, (PresentAt으로 월드에 놓였다가) 소켓에 붙어 있다가 Launch로 날아감
 * -진행 단계(EProjectilePhase)는 복제되며 BP 이벤트 OnProjectilePhaseChanged로 소유자별 연출(등장 이펙트 등)을 붙임. 데디 서버에서는 호출 안 함
 * -히트 판정은 UProjectileHitDetectionComponent(전략), 판정 결과 처리는 자신(IHitDetectionUser)이 담당
 *  어빌리티가 아니라 투사체가 데미지를 적용하므로, 발사 후 공격 어빌리티가 끝나거나 취소돼도 판정이 유지됨
 * -공격 데이터 출처(ResolveAttackData)와 아군 판정(IsFriendlyActor)은 소유자 종류별 자식 클래스가 결정
 * -서버가 판정·착탄을 처리하고, 이동은 복제 + 각 클라이언트 ProjectileMovement 시뮬레이션, 착탄 연출은 멀티캐스트
 */
UCLASS()
class ACTIONPRACTICE_API ABaseProjectile : public AActor, public IHitDetectionUser
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	ABaseProjectile();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

	//서버 전용. 데미지 GE 출처와 공격 식별 정보(AttackTags·콤보 인덱스)를 받아 히트 판정 준비
	void InitProjectile(UBaseAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageEffect, float InEffectLevel, const FGameplayTagContainer& InAttackTags, int32 InComboIndex);

	//서버 전용. 잡기 전 월드 위치에 놓음 (충돌·이동 없음, Presented 단계)
	void PresentAt(const FVector& Location, const FRotator& Rotation);

	//서버 전용. 발사 전 들고 있을 소켓에 부착 (충돌·이동 없음, Held 단계)
	//SecondarySocketName을 주면 두 소켓 중간 위치를 매 틱 따라감 (양손으로 드는 투사체). 부착 기준은 SocketName
	void AttachToSocket(USceneComponent* Parent, FName SocketName, FName SecondarySocketName = NAME_None);

	//서버 전용. 소켓에서 떼어 지정 속도로 발사하고 히트 판정 시작 (Launched 단계)
	void Launch(const FVector& LaunchVelocity);

	EProjectilePhase GetPhase() const { return Phase; }

	//발사 속도 계산용 중력 가속도 Z (ProjectileGravityScale 반영)
	float GetProjectileGravityZ() const;

	//공격 데이터 출처. 기본은 없음, 소유자 종류별 자식 클래스가 override
	virtual bool ResolveAttackData(const FGameplayTagContainer& InAttackTags, int32 InComboIndex, FFinalAttackData& OutAttackData) const { return false; }

	//아군 판정 (true면 히트에서 제외). 기본은 아군 없음, 소유자 종류별 자식 클래스가 override
	virtual bool IsFriendlyActor(const AActor* HitActor) const { return false; }

#pragma endregion

protected:
#pragma region "Protected Variables"

	//===== 컴포넌트 =====
	//이동 충돌용 루트 구체 (월드 지형·구조물만 Block, 캐릭터는 히트 판정 컴포넌트가 담당)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComponent = nullptr;

	//메시 회전·스케일 기준점. 메시 기하 중심에 두고 회전·스케일은 여기서 조정 (중심 위치 유지)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MeshPivot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileHitDetectionComponent> HitDetectionComponent = nullptr;

	//===== 투사체 속성 =====
	//캐릭터 직격 시 그 자리에서 착탄 처리 (false면 관통하며 비행 경로상 대상 전부 판정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bStopOnCharacterHit = true;

	//착탄 지점 반경 피해 (0이면 없음). 비행 중 이미 맞은 대상은 제외
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ImpactRadius = 0.0f;

	//발사 후 최대 비행 시간 (초). 지나면 착탄 없이 제거
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxFlightTime = 5.0f;

	//착탄 연출 (Niagara·사운드·카메라 쉐이크). Location/Normal은 착탄 시 채움
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FEnvironmentImpactRequest ImpactEffect;

	//ImpactEffect와 같은 착탄 위치·법선으로 함께 스폰할 추가 연출 레이어 (먼지 등). 사운드·카메라 쉐이크는 보통 ImpactEffect에만 둠
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TArray<FEnvironmentImpactRequest> AdditionalImpactEffects;

	//===== 히트 판정 =====
	UPROPERTY()
	FHitDetectionSetter HitDetectionSetter;

	//===== 데미지 출처 (서버) =====
	TWeakObjectPtr<UBaseAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffect;

	float EffectLevel = 1.0f;
	FGameplayTagContainer AttackTags;
	int32 ComboIndex = 0;

	//===== 상태 =====
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Projectile")
	EProjectilePhase Phase = EProjectilePhase::None;

	//양손 들기 두 번째 소켓 (부착 소켓과의 중간 위치를 각 머신이 매 틱 계산, 부착 자체는 부착 복제를 따름)
	UPROPERTY(ReplicatedUsing = OnRep_HoldSecondarySocketName)
	FName HoldSecondarySocketName = NAME_None;

#pragma endregion

#pragma region "Protected Functions"

	//===== IHitDetectionUser =====
	virtual void SetHitDetectionConfig() override;
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) override;

	//ProjectileMovement가 월드 충돌로 멈춤
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	//서버 전용. 판정 종료 → 착탄 반경 피해 → 착탄 연출 → 숨김 후 제거 예약
	virtual void Impact(const FHitResult& ImpactResult);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayImpactEffect(FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal);

	//===== 단계 =====
	//서버 전용. 단계 변경 + 서버(리슨·스탠드얼론) 쪽 연출 이벤트 호출
	void SetPhase(EProjectilePhase NewPhase);

	UFUNCTION()
	void OnRep_Phase(EProjectilePhase OldPhase);

	UFUNCTION()
	void OnRep_HoldSecondarySocketName();

	//Held 단계 + 두 번째 소켓이 있을 때만 액터 틱 활성 (두 소켓 중간 추적)
	void RefreshHoldTick();

	//비행 이동 충돌 활성화 (서버 Launch와 클라이언트 Launched 수신 양쪽. 콜리전 상태는 복제되지 않음)
	void EnableFlightCollision();

	//데디 서버가 아닐 때만 BP 연출 이벤트 호출
	void NotifyPhaseChanged(EProjectilePhase NewPhase, EProjectilePhase OldPhase);

	//단계 변경 연출 (등장 이펙트, 잡을 때 받침 제거 등). 클라이언트는 단계를 건너뛰어 받을 수 있으므로(None → Held 등) OldPhase와 함께 처리
	//발사 전 어빌리티 취소로 투사체가 바로 제거될 수 있어, 별도로 생성한 연출 액터는 EndPlay에서도 정리할 것
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile", meta = (DisplayName = "On Projectile Phase Changed"))
	void K2_OnProjectilePhaseChanged(EProjectilePhase NewPhase, EProjectilePhase OldPhase);

#pragma endregion
};
