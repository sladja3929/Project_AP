#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "AttackData.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EAttackDamageType : uint8
{
	None UMETA(DisplayName = "None"),
	Slash UMETA(DisplayName = "Slash"),
	Strike UMETA(DisplayName = "Strike"),
	Pierce UMETA(DisplayName = "Pierce")
};

//개별 공격에서 사용할 소켓 설정 (소켓 이름 + Radius)
USTRUCT(BlueprintType)
struct FAttackSocketConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket", meta = (GetOptions = "GetSocketGroupNames"))
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	float TraceRadius = 10.0f;
};

//개별 공격 데이터
USTRUCT(BlueprintType)
struct FAttackStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	EAttackDamageType DamageType = EAttackDamageType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TArray<FAttackSocketConfig> UsingSocketConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float PoiseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float StaminaCost = 10.0f;
	//필요에 따라 경직도, 사운드, 파티클 이펙트 등의 데이터를 여기에 추가
};

//하나의 콤보 단위 (몽타주 - 데이터 - 보조몽타주를 하나로 묶음)
USTRUCT(BlueprintType)
struct FComboAttackUnit
{
	GENERATED_BODY()

	//메인 공격 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	//이 콤보의 공격 데이터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FAttackStats AttackData;

	//보조 몽타주 (차지 액션 등, 필요한 경우만 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> SubAttackMontage;
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

//Hit Detection 소켓 정보
USTRUCT(BlueprintType)
struct FHitSocketInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	FName HitSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	int32 HitSocketCount = 2;
};