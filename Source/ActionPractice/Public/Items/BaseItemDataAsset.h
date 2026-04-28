#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BaseItemDataAsset.generated.h"

class UTexture2D;

UCLASS(Abstract, BlueprintType)
class ACTIONPRACTICE_API UBaseItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info")
	TSoftObjectPtr<UTexture2D> Icon;

	//아이템 분류 태그 (예: Item.Usable.Consumable, Item.Weapon)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info")
	FGameplayTag ItemTag;

#pragma endregion

#pragma region "Public Functions"
#pragma endregion

protected:
#pragma region "Protected Variables"
#pragma endregion

#pragma region "Protected Functions"
#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
