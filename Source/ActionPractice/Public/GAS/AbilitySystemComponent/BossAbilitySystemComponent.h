#pragma once

#include "CoreMinimal.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "BossAbilitySystemComponent.generated.h"

class ABossCharacter;
class UBossAttributeSet;

UCLASS()
class ACTIONPRACTICE_API UBossAbilitySystemComponent : public UBaseAbilitySystemComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UBossAbilitySystemComponent();

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	UFUNCTION(BlueprintPure, Category="Attributes")
	const UBossAttributeSet* GetBossAttributeSet() const;

	//===== Death =====
	virtual void HandleDeath() override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<ABossCharacter> CachedBossCharacter;

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