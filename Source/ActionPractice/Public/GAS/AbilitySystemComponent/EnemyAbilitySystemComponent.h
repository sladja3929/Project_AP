#pragma once

#include "CoreMinimal.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "EnemyAbilitySystemComponent.generated.h"

class AEnemyCharacter;
class UEnemyAttributeSet;

UCLASS()
class ACTIONPRACTICE_API UEnemyAbilitySystemComponent : public UBaseAbilitySystemComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UEnemyAbilitySystemComponent();

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	UFUNCTION(BlueprintPure, Category="Attributes")
	const UEnemyAttributeSet* GetEnemyAttributeSet() const;

	//===== Death =====
	virtual void HandleDeath() override;

	//===== Defense Policy Override =====
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;
	virtual void HandleOnDamagedResolved(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<AEnemyCharacter> CachedEnemyCharacter;

	//그로기 게이지 감소 비율 (PoiseDamage의 이 비율만큼 Stamina 차감)
	float GroggyDamageRate = 0.5f;

	FGameplayTag EnemyAbilityGroggyTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;

	//그로기 발동 여부 체크
	bool ShouldActivateGroggy() const;

	//그로기 어빌리티 활성화
	void ActivateGroggyAbility(const FFinalAttackData& FinalAttackData);

#pragma endregion

private:
#pragma region "Private Variables"


#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
