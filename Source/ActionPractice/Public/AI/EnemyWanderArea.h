#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyWanderArea.generated.h"

class UBoxComponent;
class APawn;

/**
 * 적이 Idle 배회(Wander)할 구역을 지정하는 레벨 배치 액터 (서버에서만 사용)
 * - 박스(위치·회전·크기)가 구역. 높이는 무시하고 수평 범위만 판정
 * - 여러 적(무리)이 같은 구역을 참조하면 한 구역 안에 묶여 배회
 * - 구성원별 목적지를 예약해 다른 구성원 목적지와 MinSeparation 안의 지점은 고르지 않음 (같은 곳으로 몰리는 것 방지)
 * - 적은 AEnemyCharacter::WanderArea(레벨 인스턴스별 지정)로 참조. 지정하지 않으면 스폰 위치 반경 배회
 */
UCLASS()
class ACTIONPRACTICE_API AEnemyWanderArea : public AActor
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	AEnemyWanderArea();

	//수평 범위 안인지 (박스 회전 반영, 높이 무시)
	bool IsInside(const FVector& Location) const;

	//다른 구성원이 예약한 목적지와 MinSeparation 안인지
	bool IsTooCloseToOthers(const APawn* Requester, const FVector& Location) const;

	//목적지 예약 (같은 구성원의 이전 예약은 교체)
	void ReservePoint(const APawn* Requester, const FVector& Location);

	//목적지 예약 해제 (배회 종료 시)
	void ReleasePoint(const APawn* Requester);

	//구역 안 수평 균일 무작위 지점 (높이는 박스 중심, 내비메시 투영은 호출 측)
	FVector GetRandomPointInside() const;

	//구역 중심
	FVector GetAreaCenter() const;

	FORCEINLINE UBoxComponent* GetAreaBox() const { return AreaBox; }
	FORCEINLINE const TMap<TWeakObjectPtr<const APawn>, FVector>& GetReservedPoints() const { return ReservedPoints; }

#pragma endregion

protected:
#pragma region "Protected Variables"

	//배회 구역 (에디터에서 크기·회전 조절)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wander")
	TObjectPtr<UBoxComponent> AreaBox;

	//다른 구성원 목적지와 유지할 최소 수평 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wander", meta = (ClampMin = "0.0"))
	float MinSeparation = 250.0f;

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

	//구성원별 현재 목적지 (런타임 전용)
	TMap<TWeakObjectPtr<const APawn>, FVector> ReservedPoints;

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
