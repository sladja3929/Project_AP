#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttributeSet.h"
#include "CharacterStatsDataAsset.generated.h"

class UAbilitySystemComponent;

/**
 * 어트리뷰트 초기화 엔트리
 * 에디터에서 FGameplayAttribute 드롭다운으로 어트리뷰트를 선택하고 값을 입력한다
 */
USTRUCT(BlueprintType)
struct FAttributeInitEntry
{
	GENERATED_BODY()

	//초기화할 어트리뷰트 (에디터 드롭다운으로 선택)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttribute Attribute;

	//설정할 BaseValue
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute")
	float Value = 0.f;
};

/**
 * 캐릭터 초기 스탯을 정의하는 데이터 에셋
 * GE 없이 SetNumericAttributeBase()로 직접 어트리뷰트를 초기화한다
 *
 * 배열 순서 규칙:
 * Max 어트리뷰트를 해당 현재값 어트리뷰트보다 먼저 배치할 것
 * 예: MaxHealth → Health, MaxStamina → Stamina, MaxPoise → Poise
 */
UCLASS(BlueprintType)
class ACTIONPRACTICE_API UCharacterStatsDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//초기화할 어트리뷰트 목록 (순서대로 적용됨)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes", meta = (TitleProperty = "Attribute"))
	TArray<FAttributeInitEntry> AttributeInitValues;

#pragma endregion

#pragma region "Public Functions"

	/**
	 * 이 DA의 모든 어트리뷰트 값을 ASC에 적용한다
	 * SetNumericAttributeBase()로 BaseValue를 직접 설정하며,
	 * 배열 순서대로 적용되므로 Max 어트리뷰트를 먼저 배치해야 한다
	 * @param ASC 대상 AbilitySystemComponent
	 */
	void ApplyInitialAttributes(UAbilitySystemComponent* ASC) const;

#pragma endregion
};
