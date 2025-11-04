#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "AttackData.generated.h"

UENUM(BlueprintType)
enum class EAttackDamageType : uint8
{
	None UMETA(DisplayName = "None"),
	Slash UMETA(DisplayName = "Slash"),
	Strike UMETA(DisplayName = "Strike"),
	Pierce UMETA(DisplayName = "Pierce")
};

//개별 공격 데이터
USTRUCT(BlueprintType)
struct FAttackStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	EAttackDamageType DamageType = EAttackDamageType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float PoiseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float StaminaCost = 10.0f;
	//필요에 따라 경직도, 사운드, 파티클 이펙트 등의 데이터를 여기에 추가
};

USTRUCT(BlueprintType)
struct FFinalAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	EAttackDamageType DamageType = EAttackDamageType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float FinalDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float PoiseDamage = 10.0f;
};