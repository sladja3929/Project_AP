#include "Effects/MaterialFadeCameraModifier.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Scene.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogMaterialFadeCameraModifier, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogMaterialFadeCameraModifier, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UMaterialFadeCameraModifier::SetFadeMaterial(UMaterialInterface* InMaterial)
{
	if (FadeMaterial == InMaterial && FadeMID) return;

	FadeMaterial = InMaterial;
	FadeMID = InMaterial ? UMaterialInstanceDynamic::Create(InMaterial, this) : nullptr;
	DEBUG_LOG(TEXT("SetFadeMaterial: %s"), *GetNameSafe(InMaterial));
}

void UMaterialFadeCameraModifier::SetVectorParameter(FName ParameterName, const FLinearColor& Value)
{
	if (!FadeMID) return;

	FadeMID->SetVectorParameterValue(ParameterName, Value);
}

void UMaterialFadeCameraModifier::BlendIn(float BlendTime)
{
	AlphaInTime = FMath::Max(BlendTime, 0.0f);
	EnableModifier();
	DEBUG_LOG(TEXT("BlendIn: %.2fs from Alpha %.2f"), AlphaInTime, Alpha);
}

void UMaterialFadeCameraModifier::BlendOut(float BlendTime)
{
	AlphaOutTime = FMath::Max(BlendTime, 0.0f);
	DisableModifier(false);
	DEBUG_LOG(TEXT("BlendOut: %.2fs from Alpha %.2f"), AlphaOutTime, Alpha);
}

void UMaterialFadeCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	if (!FadeMID) return;

	//선형 Alpha를 부드럽게 가감속 (블렌더블 가중치 대신 머티리얼 강도로 전달)
	const float Intensity = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	if (Intensity <= 0.0f) return;

	FadeMID->SetScalarParameterValue(IntensityParameterName, Intensity);
	PostProcessSettings.AddBlendable(FadeMID, 1.0f);
	PostProcessBlendWeight = 1.0f;
}
