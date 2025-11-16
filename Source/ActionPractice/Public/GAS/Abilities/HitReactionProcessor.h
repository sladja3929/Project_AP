#pragma once

#include "CoreMinimal.h"
#include "HitReactionProcessor.generated.h"

UENUM(BlueprintType)
enum class EReactionLevel : uint8
{
	None UMETA(DisplayName = "None"),
	Light UMETA(DisplayName = "Light"),
	Middle UMETA(DisplayName = "Middle"),
	Heavy UMETA(DisplayName = "Heavy"),
};

USTRUCT(BlueprintType)
struct ACTIONPRACTICE_API FHitReactionProcessor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction")
	float LightThreshold = -20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction")
	float HeavyThreshold = -50.0f;

	FHitReactionProcessor();

	void InitReactionLevel(float Light, float Heavy);

	void SelectReactionLevel(float PoiseDamage);

	EReactionLevel GetReactionLevel() const { return ReactionLevel; }

private:
	EReactionLevel ReactionLevel = EReactionLevel::None;
};