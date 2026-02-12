#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ActionPracticeGameMode.generated.h"

class ABossCharacter;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AActionPracticeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Boss")
	TSubclassOf<ABossCharacter> BossClass;

	UPROPERTY(EditDefaultsOnly, Category = "Boss")
	FVector BossSpawnLocation = FVector(0, 0, 100);

	UPROPERTY(EditDefaultsOnly, Category = "Boss")
	FRotator BossSpawnRotation = FRotator::ZeroRotator;

	/** Constructor */
	AActionPracticeGameMode();

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void BeginPlay() override;

protected:
	UPROPERTY()
	TObjectPtr<ABossCharacter> SpawnedBoss = nullptr;
};



