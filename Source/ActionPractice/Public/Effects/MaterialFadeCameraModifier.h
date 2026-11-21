#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "MaterialFadeCameraModifier.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

//포스트 프로세스 머티리얼을 모디파이어 Alpha에 맞춰 화면에 섞는 카메라 모디파이어
//PlayerCameraManager 소속이라 로컬 플레이어 화면에만 적용됨 (데디 서버·다른 클라 영향 없음)
//Alpha를 이징해 머티리얼 강도 파라미터로 전달하고, BlendOut 후 Alpha가 0이 되면 스스로 비활성화
UCLASS()
class ACTIONPRACTICE_API UMaterialFadeCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	//머티리얼 지정, 기존과 다르면 MID 재생성 (같은 머티리얼 재사용 시 이전에 설정한 파라미터 유지)
	void SetFadeMaterial(UMaterialInterface* InMaterial);

	//MID 벡터 파라미터 설정 (SetFadeMaterial 이후 호출)
	void SetVectorParameter(FName ParameterName, const FLinearColor& Value);

	//현재 Alpha에서 1까지 BlendTime 동안 진입
	void BlendIn(float BlendTime);

	//현재 Alpha에서 0까지 BlendTime 동안 복귀, 0 도달 시 비활성화
	void BlendOut(float BlendTime);

#pragma endregion

protected:
#pragma region "Protected Variables"

	//머티리얼 강도 스칼라 파라미터 이름 (페이드 머티리얼 공통 규약)
	static inline const FName IntensityParameterName = TEXT("Intensity");

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FadeMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FadeMID = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

#pragma endregion
};
