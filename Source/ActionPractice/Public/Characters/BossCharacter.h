#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyCharacter.h"
#include "BossCharacter.generated.h"

class AActionPracticePlayerController;
class ABossArena;

UCLASS()
class ACTIONPRACTICE_API ABossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	ABossCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Network =====
	//모든 클라이언트에서 보스 조우 연출 (BGM, UI)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnBossEncounter();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnBossDisengage();

	// ===== Enemy Reset (보스 전용 추가) =====
	virtual void ResetEnemy() override;

	//타깃이 보스룸 안에 있는지 (보스룸이 LoseSightRadius보다 넓어도 안에서는 전투 유지)
	virtual bool IsTargetInCombatArea(const AActor* Target) const override;

	//보스는 머리 위 HP바를 사용하지 않음 (화면 하단 보스 HP바 사용)
	virtual void ShowEnemyHealthBar() override;
	virtual void HideEnemyHealthBar() override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//보스 조우 중인지 서버측 플래그
	bool bBossEncountered = false;

	//보스룸 (레벨 배치 인스턴스별 지정). 플레이어가 들어오면 시야와 무관하게 즉시 인지
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<ABossArena> BossArena;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> BossBGM;

	UPROPERTY()
	TObjectPtr<class UAudioComponent> BGMAudioComponent;

#pragma endregion

#pragma region "Protected Functions"

	// ===== Combat State (보스 전용 override) =====
	//전투 진입 시 조우 연출, 해제 시 이탈 연출 (시야를 잠깐 벗어나도 유지)
	virtual void OnCombatStateChanged(bool bInCombat, AActor* Target) override;

	// ===== Audio =====
	void PlayBossBGM();
	void StopBossBGM();

	// ===== Boss Arena =====
	//보스룸 진입 알림 → AI 컨트롤러에 타깃 인지 요청 (서버 전용)
	void OnPlayerEnteredArena(AActor* Player);

	//이미 보스룸 안에 있는 플레이어 인지 (진입 이벤트를 놓친 경우: 바인딩 전 스폰, 리셋 후 잔류)
	void PerceivePlayersInArena();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
