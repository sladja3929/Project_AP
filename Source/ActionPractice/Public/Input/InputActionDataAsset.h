#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "InputActionDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FInputActionAbilityRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTagContainer AbilityAssetTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Buffer")
    bool bCanBuffered = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Buffer")
    int BufferPriority = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Buffer")
    bool bIsHoldAction = false;

    //RPC용 태그 식별자 - 네트워크에서 InputAction 대신 사용
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network")
    FGameplayTag InputActionTag;
};

UCLASS(BlueprintType)
class ACTIONPRACTICE_API UInputActionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
#pragma region "Public Variables"

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TMap<TObjectPtr<UInputAction>, FInputActionAbilityRule> Rules;

#pragma endregion

#pragma region "Public Functions"

    const FInputActionAbilityRule* FindRuleByAction(const UInputAction* InputAction) const
    {
        if (!InputAction)
        {
            UE_LOG(LogTemp, Warning, TEXT("FindRuleByAction: InAction is null"));
            return nullptr;
        }

        if (const FInputActionAbilityRule* Found = Rules.Find(InputAction))
        {
            return Found;
        }

        UE_LOG(LogTemp, Warning, TEXT("FindRuleByAction: Rule not found for Action=%s"), *GetNameSafe(InputAction));
        return nullptr;
    }

    //태그로 InputAction 찾기
    const UInputAction* FindInputActionByTag(FGameplayTag Tag) const
    {
        if (!Tag.IsValid())
        {
            return nullptr;
        }

        for (const auto& Pair : Rules)
        {
            if (Pair.Value.InputActionTag == Tag)
            {
                return Pair.Key;
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("FindInputActionByTag: InputAction not found for Tag=%s"), *Tag.ToString());
        return nullptr;
    }

    //InputAction으로 태그 찾기
    FGameplayTag FindTagByInputAction(const UInputAction* InputAction) const
    {
        if (!InputAction)
        {
            return FGameplayTag();
        }

        if (const FInputActionAbilityRule* Rule = FindRuleByAction(InputAction))
        {
            return Rule->InputActionTag;
        }

        return FGameplayTag();
    }

    //태그로 홀드 액션 여부 확인
    bool IsHoldActionByTag(FGameplayTag Tag) const
    {
        const UInputAction* InputAction = FindInputActionByTag(Tag);
        if (InputAction)
        {
            if (const FInputActionAbilityRule* Rule = FindRuleByAction(InputAction))
            {
                return Rule->bIsHoldAction;
            }
        }
        return false;
    }

#pragma endregion
};
