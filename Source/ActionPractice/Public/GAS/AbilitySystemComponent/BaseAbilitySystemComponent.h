#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DefensePolicy.h"
#include "BaseAbilitySystemComponent.generated.h"

class ABaseCharacter;
class UAttributeSet;
struct FFinalAttackData;
struct FActionPracticeGameplayEffectContext;

/**
 * Base AbilitySystemComponent
 * ActionPracticeAbilitySystemComponent와 BossAbilitySystemComponent의 공통 기능
 */
UCLASS()
class ACTIONPRACTICE_API UBaseAbilitySystemComponent : public UAbilitySystemComponent, public IDefensePolicy
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UBaseAbilitySystemComponent();

	//ASC가 Owner/Avatar 정보를 수집하는 지점
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	//초기화 완료 델리게이트(필요 시 외부에서 바인딩)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnASCInitialized, UBaseAbilitySystemComponent*);
	FOnASCInitialized OnASCInitialized;

	//기본 GE 생성 헬퍼
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	FGameplayEffectSpecHandle CreateGameplayEffectSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, UObject* SourceObject = nullptr);

	//공격 GE 생성
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	FGameplayEffectSpecHandle CreateAttackGameplayEffectSpec(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		float Level,
		UObject* SourceObject,
		const FFinalAttackData& FinalAttackData
	);

	//SetByCaller 설정
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	void SetSpecSetByCallerMagnitude(FGameplayEffectSpecHandle& SpecHandle, const FGameplayTag& Tag, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	void SetSpecSetByCallerMagnitudes(FGameplayEffectSpecHandle& SpecHandle, const TMap<FGameplayTag, float>& Magnitudes);

	//===== Defense Policy Interface =====
	UFUNCTION()
	virtual void OnDamaged(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

	UFUNCTION()
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

	UFUNCTION()
	virtual void HandleOnDamagedResolved(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

	virtual void PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<ABaseCharacter> CachedCharacter;

	FGameplayTag AbilityHitReactionTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};