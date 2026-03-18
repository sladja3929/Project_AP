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

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<AEnemyCharacter> CachedEnemyCharacter;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;

#pragma endregion

private:
#pragma region "Private Variables"


#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
