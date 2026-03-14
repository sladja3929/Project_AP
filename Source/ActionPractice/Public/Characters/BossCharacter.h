#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAIController.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/BossAttributeSet.h"
#include "BossCharacter.generated.h"

class AActionPracticeCharacter;
class AActionPracticePlayerController;
class UEnemyAttackComponent;
class UEnemyDataAsset;
class UGameplayEffect;

UCLASS()
class ACTIONPRACTICE_API ABossCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FName EnemyName = NAME_None;

#pragma endregion 

#pragma region "Public Functions"

	ABossCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Hit Detection Interface =====
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const override;

	FORCEINLINE UBossAttributeSet* GetAttributeSet() const { return Cast<UBossAttributeSet>(AttributeSet); }
	FORCEINLINE class AEnemyAIController* GetEnemyAIController() const { return Cast<AEnemyAIController>(GetController()); }

	const UEnemyDataAsset* GetEnemyData() const { return EnemyData.Get(); }

	void RotateToTarget(const AActor* TargetActor, float RotateTime);

	// ===== Enemy Reset  =====
	//BeginPlay 시점에 초기 스폰 상태를 캐시
	void CacheInitialEnemyState();

	//GameMode에서 호출
	void ResetEnemy();

	// ===== Replication =====
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TObjectPtr<UEnemyDataAsset> EnemyData;
	
	UPROPERTY(EditDefaultsOnly, Category = "Reset")
	TSubclassOf<UGameplayEffect> EnemyResetRecoveryEffect;

	//보스 조우 중인지 서버측 플래그
	bool bBossEncountered = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UEnemyAttackComponent> EnemyAttackComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> BossBGM;

	UPROPERTY()
	TObjectPtr<class UAudioComponent> BGMAudioComponent;

#pragma endregion

#pragma region "Protected Functions"

	// ===== GAS =====
	virtual void CreateAbilitySystemComponent() override;
	virtual void CreateAttributeSet() override;

	// ===== Enemy Reset Helpers =====
	void ApplyEnemyResetEffect();
	void RestartEnemyAI();

	// ===== UI =====
	UFUNCTION()
	void OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus);

	// ===== Audio =====
	void PlayBossBGM();
	void StopBossBGM();

	// ===== Network =====
	//모든 클라이언트에서 보스 조우 연출 (BGM, UI)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnBossEncounter();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnBossDisengage();

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AActionPracticeCharacter> DetectedPlayer;

	//초기 스폰 Transform 캐시
	FTransform InitialTransform;

#pragma endregion

#pragma region "Private Functions"


#pragma endregion
};