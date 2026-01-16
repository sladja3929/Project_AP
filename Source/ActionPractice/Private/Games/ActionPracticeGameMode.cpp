#include "Public/Games/ActionPracticeGameMode.h"
#include "Characters/BossCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AActionPracticeGameMode::AActionPracticeGameMode()
{
	// 기본 설정
}

void AActionPracticeGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 보스 스폰 (싱글플레이어에서는 항상 true)
	if (HasAuthority() && BossClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		SpawnedBoss = GetWorld()->SpawnActor<ABossCharacter>(
			BossClass,
			BossSpawnLocation,
			BossSpawnRotation,
			SpawnParams
		);

		if (SpawnedBoss)
		{
			UE_LOG(LogTemp, Log, TEXT("Boss spawned: %s at location: %s"), *SpawnedBoss->GetName(), *BossSpawnLocation.ToString());
		}
	}
}

AActor* AActionPracticeGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 모든 PlayerStart 찾기
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);

	if (PlayerStarts.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No PlayerStart found!"));
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// 플레이어 수에 따라 다른 위치 할당 (간단한 라운드로빈)
	int32 NumPlayers = GetNumPlayers();
	int32 StartIndex = (NumPlayers - 1) % PlayerStarts.Num();

	UE_LOG(LogTemp, Log, TEXT("Player %d spawning at PlayerStart index %d"), NumPlayers, StartIndex);
	return PlayerStarts[StartIndex];
}
