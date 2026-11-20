#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyPackSubsystem.generated.h"

class AActor;
class AEnemyAIController;
class AEnemyCharacter;
class UEnemyPack;

/**
 * 적 무리 전투 조정자 (서버 전용, 게임 월드에만 생성)
 * - 무리 소집: 무리 전투를 쓰는 적(EnemyDataAsset::PackSettings)이 전투에 진입하면 같은 배회 구역 구성원(구역이 없으면 RallyRadius 안 같은 종류)을 같은 타깃으로 전투에 진입시킴
 *   이미 같은 타깃과 싸우는 무리가 있으면 그 무리에 합류
 * - 공격권 갱신: 주기적으로 무리별 공격자를 다시 지정 (구성원 변화 시에는 즉시)
 * - 구성원 상태(전투 진입·해제)의 소유자는 여전히 EnemyAIController이며, 이 서브시스템은 무리 단위 판단만 담당
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyPackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	//전투에 진입한 컨트롤러를 무리에 넣고, 아직 전투가 아닌 무리원을 같은 타깃으로 전투 진입시킴 (무리 미사용 적·이미 소속이면 무시)
	void RallyPack(AEnemyAIController* Instigator, AActor* Target);

	//무리 탈퇴 (전투 해제·사망·리셋·빙의 해제). 빈 무리는 제거
	void LeavePack(AEnemyAIController* Member);

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEnemyPack>> Packs;

	//공격권 갱신 주기 (초)
	float AttackerUpdateInterval = 0.2f;

	FTimerHandle AttackerUpdateTimer;

#pragma endregion

#pragma region "Private Functions"

	//같은 타깃과 싸우는 합류 가능한 무리 (같은 구역, 구역이 없으면 같은 종류이면서 소집 반경 안에 구성원이 있는 무리)
	UEnemyPack* FindJoinablePack(const AEnemyCharacter* Enemy, const AActor* Target) const;

	//아직 전투가 아닌 소집 대상 (같은 구역, 구역이 없으면 같은 종류·구역 없음·반경 안)
	void GatherRallyCandidates(const AEnemyCharacter* Instigator, float RallyRadius, TArray<AEnemyAIController*>& OutCandidates) const;

	void JoinPack(UEnemyPack* Pack, AEnemyAIController* Member);

	void RemovePack(UEnemyPack* Pack);

	//타이머 콜백: 무효 구성원 정리, 빈 무리 제거, 공격자 갱신
	void UpdatePacks();

	//무리 전투를 쓰는 적인지
	static bool UsesPack(const AEnemyCharacter* Enemy);

#pragma endregion
};
