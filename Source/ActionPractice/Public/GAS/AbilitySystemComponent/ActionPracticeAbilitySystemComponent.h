#pragma once

#include "CoreMinimal.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "ActionPracticeAbilitySystemComponent.generated.h"

class AActionPracticeCharacter;
class UActionPracticeAttributeSet;

UCLASS()
class ACTIONPRACTICE_API UActionPracticeAbilitySystemComponent : public UBaseAbilitySystemComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UActionPracticeAbilitySystemComponent();

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	UFUNCTION(BlueprintPure, Category="Attributes")
	const UActionPracticeAttributeSet* GetActionPracticeAttributeSet() const;

	//WaitInputRelease / WaitInputPress 계열 태스크용 입력 이벤트 브릿지
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	
	//===== Defense Policy Override =====
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;
	virtual void PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<AActionPracticeCharacter> CachedAPCharacter;
	FGameplayTag EffectStaminaRegenBlockDurationTag;
	FGameplayTag StateAbilityBlockingTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
#pragma region "Private Variables"

	FActiveGameplayEffectHandle StaminaRegenBlockHandle;

	//블로킹 관련 변수
	bool bBlockedLastAttack = false;

#pragma endregion

#pragma region "Private Functions"

	void CheckBlockSuccess(AActor* SourceActor);

#pragma endregion
};
