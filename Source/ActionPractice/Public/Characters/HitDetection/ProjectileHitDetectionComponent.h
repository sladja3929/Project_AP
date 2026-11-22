#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "ProjectileHitDetectionComponent.generated.h"

class ABaseProjectile;

/**
 * 투사체용 히트 판정 전략 (IHitDetectionInterface 구현)
 *
 * -Prepare: 소유 투사체(ABaseProjectile)를 통해 공격 데이터를 조립해 보관 (데이터 출처는 투사체 자식 클래스가 결정)
 * -Start/End: 애님 노티파이 이벤트가 아니라 투사체가 발사·착탄 시 직접 호출 (공격자 ASC의 HitDetection 이벤트와 무관)
 * -판정: 매 틱 이전 위치 → 현재 위치를 구체로 WeaponTrace 스윕. 근접 공격과 같은 Physics Asset 바디 기준이며 PhysMat이 Impact 큐로 전달됨
 * -검증: 투사체·Instigator 제외, 투사체의 아군 판정 제외, 액터당 1회
 * -서버(Authority) 전용
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UProjectileHitDetectionComponent : public UActorComponent, public IHitDetectionInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	FOnHitDetected OnHit;

	//판정 구체 반경 (투사체 외형 크기에 맞춤)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile Hit Detection", meta = (ClampMin = "1.0"))
	float TraceRadius = 30.0f;

#pragma endregion

#pragma region "Public Functions"

	UProjectileHitDetectionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//===== HitDetection Interface =====
	virtual void PrepareHitDetection(const FGameplayTagContainer& AttackTags, const int32 ComboIndex) override;
	virtual void PrepareHitDetection(const FName& AttackName, const int32 ComboIndex) override { }
	virtual void HandleHitDetectionStart(const FGameplayEventData& Payload) override;
	virtual void HandleHitDetectionEnd(const FGameplayEventData& Payload) override;
	virtual FOnHitDetected& GetOnHitDetected() override { return OnHit; }
	//=====================================

	//마지막 판정 위치부터 현재 위치까지 남은 구간 스윕 (착탄 직전 프레임 구간 누락 방지)
	void SweepToCurrentLocation();

	//반경 안 판정 바디 일괄 판정 (착탄 반경 피해). 이미 맞은 액터는 제외
	void DetectRadius(const FVector& Center, float Radius);

	bool IsDetecting() const { return bIsDetecting; }

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<ABaseProjectile> OwnerProjectile = nullptr;

	FFinalAttackData CurrentAttackData;

	//이미 맞은 액터 (투사체 1개당 액터별 1회)
	TSet<TWeakObjectPtr<AActor>> HitActors;

	FVector LastTraceLocation = FVector::ZeroVector;

	bool bIsPrepared = false;
	bool bIsDetecting = false;

#pragma endregion

#pragma region "Protected Functions"

	void ProcessHits(const TArray<FHitResult>& Hits);
	bool ValidateHit(const AActor* HitActor) const;
	FCollisionQueryParams GetCollisionQueryParams() const;

#pragma endregion

#pragma region "Debug And Profiling"
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugTrace")
	bool bDrawDebugTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugTrace")
	float DebugTraceDuration = 3.0f;

#pragma endregion
};
