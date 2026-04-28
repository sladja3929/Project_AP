// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EffectManagerSubsystem.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UCameraShakeBase;

USTRUCT(BlueprintType)
struct FEnvironmentImpactRequest
{
	GENERATED_BODY()

	//스폰할 Niagara 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> NiagaraEffect = nullptr;

	//스폰할 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TObjectPtr<USoundBase> Sound = nullptr;

	//월드 스폰 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector Location = FVector::ZeroVector;

	//충돌 표면 법선 (bAlignToNormal이 true일 때 이펙트 방향 결정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector Normal = FVector::UpVector;

	//Location 기준 추가 오프셋 (최종 회전 기준 로컬 공간)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector LocationOffset = FVector::ZeroVector;

	//추가 회전 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FRotator RotationOffset = FRotator::ZeroRotator;

	//이펙트 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector Scale = FVector::OneVector;

	//true면 Normal 방향으로 이펙트를 정렬한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	bool bAlignToNormal = true;

	//사운드 볼륨 배수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float VolumeMultiplier = 1.0f;

	//사운드 피치 배수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float PitchMultiplier = 1.0f;

	// ===== Camera Shake =====

	//재생할 카메라 쉐이크 클래스 (BP_CameraShake 등)
	//nullptr이면 카메라 쉐이크를 재생하지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraShakeClass = nullptr;

	//거리 감쇠 곡선 지수 (1.0 = 선형, 2.0 = 급격 감쇠)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.01"))
	float CameraShakeFalloff = 1.0f;

	//이 반경 내에서는 쉐이크 100% 적용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float CameraShakeInnerRadius = 0.0f;

	//이 반경 밖이면 카메라 쉐이크 없음 (InnerRadius ~ OuterRadius 구간에서 선형 감쇠)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float CameraShakeOuterRadius = 1500.0f;
};

//전역 이펙트 관리 서브시스템
//GAS GameplayCue와 독립적으로 이펙트/사운드를 스폰한다.
//환경 충돌, 발자국, 날씨 등 비GAS 이펙트의 중앙 진입점.
//Dedicated Server에서는 이펙트/사운드 스폰을 건너뛴다.
UCLASS()
class ACTIONPRACTICE_API UEffectManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	//월드 컨텍스트로 서브시스템 인스턴스를 가져온다
	static UEffectManagerSubsystem* Get(const UObject* WorldContextObject);

	//환경 충돌 이펙트를 월드 위치에 스폰한다
	//Request의 Location/Normal 기반으로 Niagara와 사운드를 스폰한다
	//Dedicated Server에서는 nullptr을 반환하고 아무것도 스폰하지 않는다
	UFUNCTION(BlueprintCallable, Category = "Effect Manager")
	UNiagaraComponent* SpawnEnvironmentImpact(const FEnvironmentImpactRequest& Request);

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

	//Normal 기반 회전을 계산한다
	//FRotationMatrix::MakeFromZ로 Z축을 Normal로 정렬한 뒤 AdditionalRotation을 합산
	FRotator CalculateAlignedRotation(const FVector& Normal, const FRotator& AdditionalRotation) const;

#pragma endregion
};
