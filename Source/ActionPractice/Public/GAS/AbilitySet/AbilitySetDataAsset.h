#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySetDataAsset.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

/**
 * AbilitySet에 포함될 어빌리티 엔트리
 */
USTRUCT(BlueprintType)
struct FAbilitySetEntry_Ability
{
	GENERATED_BODY()

	//부여할 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	//어빌리티 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	int32 AbilityLevel = 1;
};

/**
 * AbilitySet에 포함될 이펙트 엔트리
 */
USTRUCT(BlueprintType)
struct FAbilitySetEntry_Effect
{
	GENERATED_BODY()

	//적용할 이펙트 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> Effect = nullptr;

	//이펙트 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	float EffectLevel = 1.0f;
};

/**
 * AbilitySet 부여 결과를 저장하는 핸들 구조체
 * RemoveFromASC로 부여된 모든 어빌리티/이펙트를 일괄 회수할 수 있다
 */
USTRUCT()
struct FAbilitySetGrantedHandles
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddEffectHandle(const FActiveGameplayEffectHandle& Handle);

	//부여된 모든 어빌리티/이펙트를 ASC에서 회수
	void RemoveFromASC(UAbilitySystemComponent* ASC);

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;

#pragma endregion
};

/**
 * 어빌리티와 이펙트를 묶어 관리하는 데이터 에셋
 * 하나의 GiveToAbilitySystem 호출로 세트 전체를 부여하고,
 * FAbilitySetGrantedHandles를 통해 일괄 회수한다
 */
UCLASS(BlueprintType)
class ACTIONPRACTICE_API UAbilitySetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//부여할 어빌리티 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities", meta = (TitleProperty = "Ability"))
	TArray<FAbilitySetEntry_Ability> GrantedAbilities;

	//적용할 이펙트 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (TitleProperty = "Effect"))
	TArray<FAbilitySetEntry_Effect> GrantedEffects;

#pragma endregion

#pragma region "Public Functions"

	/**
	 * 이 세트의 모든 어빌리티/이펙트를 ASC에 부여한다
	 * @param ASC 대상 AbilitySystemComponent
	 * @param OutHandles 부여 결과 핸들 (회수 시 사용, nullptr 가능)
	 * @param SourceObject GE EffectContext의 SourceObject
	 */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FAbilitySetGrantedHandles* OutHandles, UObject* SourceObject = nullptr) const;

#pragma endregion
};
