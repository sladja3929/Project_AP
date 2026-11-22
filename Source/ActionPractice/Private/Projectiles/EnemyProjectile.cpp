#include "Projectiles/EnemyProjectile.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyProjectile, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyProjectile, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool AEnemyProjectile::ResolveAttackData(const FGameplayTagContainer& InAttackTags, int32 InComboIndex, FFinalAttackData& OutAttackData) const
{
	const AEnemyCharacter* OwnerEnemy = Cast<AEnemyCharacter>(GetInstigator());
	const UEnemyDataAsset* EnemyData = OwnerEnemy ? OwnerEnemy->GetEnemyData() : nullptr;

	if (!EnemyData)
	{
		DEBUG_LOG(TEXT("ResolveAttackData: Instigator is not an enemy with EnemyData (%s)"), *GetNameSafe(GetInstigator()));
		return false;
	}

	return EnemyData->BuildFinalAttackData(InAttackTags, InComboIndex, OutAttackData);
}

bool AEnemyProjectile::IsFriendlyActor(const AActor* HitActor) const
{
	return HitActor && HitActor->IsA<AEnemyCharacter>();
}
