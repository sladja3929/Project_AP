#pragma once

#include "CoreMinimal.h"
#include "Characters/HitDetection/AttackTraceComponent.h"
#include "EnemyAttackComponent.generated.h"

class AEnemyCharacter;
class USkeletalMeshComponent;
class UAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UEnemyAttackComponent : public UAttackTraceComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UEnemyAttackComponent();

	virtual void BeginPlay() override;

	virtual AActor* GetOwnerActor() const override;
	virtual UAbilitySystemComponent* GetOwnerASC() const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<AEnemyCharacter> OwnerEnemy = nullptr;

#pragma endregion

#pragma region "Protected Functions"
	
	virtual bool LoadTraceConfig(const FGameplayTagContainer& AttackTags, int32 ComboIndex) override;

	virtual void SetOwnerMesh() override;

	virtual void AddIgnoredActors(FCollisionQueryParams& Params) const override;

	//다른 적은 아군 (무리 전투에서 플레이어를 둘러싼 적끼리 공격이 들어가는 것 방지)
	virtual bool IsFriendlyActor(const AActor* HitActor) const override;

#pragma endregion
};