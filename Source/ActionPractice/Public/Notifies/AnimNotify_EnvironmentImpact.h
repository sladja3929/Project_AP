// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EnvironmentImpact.generated.h"

class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

UENUM(BlueprintType)
enum class EEnvironmentTraceDirection : uint8
{
	WorldDown     UMETA(DisplayName = "World Down (-Z)"),
	ActorForward  UMETA(DisplayName = "Actor Forward"),
	ActorDown     UMETA(DisplayName = "Actor Down (-Z Local)"),
	Custom        UMETA(DisplayName = "Custom (Local Space)"),
};

UCLASS(meta = (DisplayName = "Environment Impact"))
class ACTIONPRACTICE_API UAnimNotify_EnvironmentImpact : public UAnimNotify
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//--- Trace ---

	//트레이스 시작점 소켓들. 각 소켓마다 개별 트레이스 수행. 빈 배열이면 액터 위치에서 1회 트레이스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	TArray<FName> TraceSocketNames;

	//트레이스 방향 프리셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	EEnvironmentTraceDirection TraceDirection = EEnvironmentTraceDirection::WorldDown;

	//Custom일 때 로컬 스페이스 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (EditCondition = "TraceDirection == EEnvironmentTraceDirection::Custom"))
	FVector CustomTraceDirection = FVector(0.0f, 0.0f, -1.0f);

	//트레이스 길이(cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceLength = 200.0f;

	//0이면 LineTrace, 0 초과면 SphereTrace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "0.0"))
	float TraceRadius = 0.0f;

	//트레이스 충돌 채널
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	//--- Effect ---

	//스폰할 Niagara 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> NiagaraEffect = nullptr;

	//히트 위치 기준 추가 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FVector LocationOffset = FVector::ZeroVector;

	//추가 회전 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FRotator RotationOffset = FRotator::ZeroRotator;

	//이펙트 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FVector Scale = FVector::OneVector;

	//히트 표면 Normal 방향으로 이펙트 정렬
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	bool bAlignToSurfaceNormal = true;

	//--- Sound ---

	//스폰할 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	TObjectPtr<USoundBase> Sound = nullptr;

	//사운드 볼륨 배수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float VolumeMultiplier = 1.0f;

	//사운드 피치 배수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float PitchMultiplier = 1.0f;

	//--- Camera Shake ---

	//재생할 카메라 쉐이크 클래스 (BP_CameraShake 등)
	//nullptr이면 카메라 쉐이크를 재생하지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraShakeClass = nullptr;

	//거리 감쇠 곡선 지수 (1.0 = 선형, 2.0 = 급격 감쇠)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.01"))
	float CameraShakeFalloff = 1.0f;

	//풀 강도 적용 내부 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float CameraShakeInnerRadius = 0.0f;

	//감쇠 외부 반경 (이 밖에서는 쉐이크 없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float CameraShakeOuterRadius = 1500.0f;

#pragma endregion

#pragma region "Public Functions"

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

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

	//소켓 위치에서 트레이스를 수행한다
	//히트 시 OutLocation/OutNormal을 채우고 true 반환
	bool PerformEnvironmentTrace(USkeletalMeshComponent* MeshComp, AActor* Owner, const FVector& TraceOrigin, FVector& OutLocation, FVector& OutNormal) const;

	//TraceDirection 설정에 따라 월드 스페이스 트레이스 방향을 계산한다
	FVector GetWorldTraceDirection(AActor* Owner) const;

#pragma endregion
};
