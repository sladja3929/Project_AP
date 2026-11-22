#pragma once

#include "CoreMinimal.h"
#include "Projectiles/BaseProjectile.h"
#include "EnemyProjectile.generated.h"

/**
 * 적이 쏘는 투사체
 *
 * -공격 데이터: Instigator(AEnemyCharacter)의 EnemyDataAsset에서 AttackTags·콤보 인덱스로 조회 (본체 근접 판정과 같은 계산)
 * -아군 판정: 다른 적 전체를 아군으로 봄 (EnemyAttackComponent와 같은 기준)
 */
UCLASS()
class ACTIONPRACTICE_API AEnemyProjectile : public ABaseProjectile
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	virtual bool ResolveAttackData(const FGameplayTagContainer& InAttackTags, int32 InComboIndex, FFinalAttackData& OutAttackData) const override;
	virtual bool IsFriendlyActor(const AActor* HitActor) const override;

#pragma endregion
};
