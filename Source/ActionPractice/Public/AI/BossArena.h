#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArena.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

//플레이어 보스룸 진입 알림 (진입한 플레이어)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBossArenaPlayerEntered, AActor* /*Player*/);

/**
 * 보스룸 범위를 지정하는 레벨 배치 액터 (서버에서만 동작)
 * - 박스(위치·회전·크기)가 보스룸 전체. 플레이어가 들어오면 OnPlayerEntered 브로드캐스트
 * - 보스는 ABossCharacter::BossArena(레벨 인스턴스별 지정)로 참조하고, 진입 알림을 받아 시야와 무관하게 즉시 타깃 인지
 * - 진입은 인지만 담당. 타깃 해제는 기존 시야·전투 해제 규칙(LoseSightRadius)을 그대로 따름
 */
UCLASS()
class ACTIONPRACTICE_API ABossArena : public AActor
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	FOnBossArenaPlayerEntered OnPlayerEntered;

#pragma endregion

#pragma region "Public Functions"

	ABossArena();

	virtual void BeginPlay() override;

	//현재 보스룸 안에 있는 플레이어 (진입 이벤트를 놓친 경우 보완: 바인딩 전 스폰, 보스 리셋 후 잔류)
	void GetOverlappingPlayers(TArray<AActor*>& OutPlayers) const;

	//액터가 보스룸 안에 있는지 (박스와 오버랩 중)
	bool IsActorInside(const AActor* Actor) const;

	FORCEINLINE UBoxComponent* GetArenaBox() const { return ArenaBox; }

#pragma endregion

protected:
#pragma region "Protected Variables"

	//보스룸 범위 (에디터에서 크기·회전 조절). Pawn과만 오버랩
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss Arena")
	TObjectPtr<UBoxComponent> ArenaBox;

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	UFUNCTION()
	void OnArenaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

#pragma endregion
};
