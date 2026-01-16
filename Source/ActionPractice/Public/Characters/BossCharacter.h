#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAIController.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/BossAttributeSet.h"
#include "BossCharacter.generated.h"

class UBossHealthWidget;
class AActionPracticeCharacter;
class UEnemyAttackComponent;
class UEnemyDataAsset;

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

	//===== Hit Detection Interface =====
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const override;

	FORCEINLINE UBossAttributeSet* GetAttributeSet() const { return Cast<UBossAttributeSet>(AttributeSet); }
	FORCEINLINE UBossHealthWidget* GetBossHealthWidget() const { return BossHealthWidget; }
	FORCEINLINE class AEnemyAIController* GetEnemyAIController() const { return Cast<AEnemyAIController>(GetController()); }

	const UEnemyDataAsset* GetEnemyData() const { return EnemyData.Get(); }

	void RotateToTarget(const AActor* TargetActor, float RotateTime);

	//===== Replication =====
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TObjectPtr<UEnemyDataAsset> EnemyData;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBossHealthWidget> BossHealthWidgetClass;

	UPROPERTY()
	TObjectPtr<UBossHealthWidget> BossHealthWidget;

	bool bHealthWidgetActive = false;

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

	// ===== UI =====
	UFUNCTION()
	void OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus);

	void CreateAndAttachHealthWidget();
	void RemoveHealthWidget();

	// ===== Audio =====
	void PlayBossBGM();
	void StopBossBGM();

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AActionPracticeCharacter> DetectedPlayer;

#pragma endregion

#pragma region "Private Functions"


#pragma endregion
};