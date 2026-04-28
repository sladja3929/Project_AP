#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyCharacter.h"
#include "BossCharacter.generated.h"

class AActionPracticePlayerController;

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

	//보스는 머리 위 HP바를 사용하지 않음 (화면 하단 보스 HP바 사용)
	virtual void ShowEnemyHealthBar() override;
	virtual void HideEnemyHealthBar() override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//보스 조우 중인지 서버측 플래그
	bool bBossEncountered = false;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> BossBGM;

	UPROPERTY()
	TObjectPtr<class UAudioComponent> BGMAudioComponent;

#pragma endregion

#pragma region "Protected Functions"

	// ===== Perception (보스 전용 override) =====
	virtual void OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus) override;

	// ===== Audio =====
	void PlayBossBGM();
	void StopBossBGM();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
