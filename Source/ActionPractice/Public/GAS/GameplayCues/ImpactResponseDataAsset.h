#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GAS/AbilitySystemComponent/DefensePolicy.h"
#include "ImpactResponseDataAsset.generated.h"

class UNiagaraSystem;
class USoundBase;

//하나의 Impact 응답 데이터 (이펙트 + 사운드 쌍)
USTRUCT(BlueprintType)
struct FImpactResponseData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> NiagaraEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<USoundBase> Sound = nullptr;
};

//가드 피격 시 단일 응답 / 일반 피격 시 Surface Type별 분기
//Block은 재질 무관하게 단일 응답, 피격은 PM에 따라 분기한다.
UCLASS(BlueprintType)
class ACTIONPRACTICE_API UImpactResponseDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//가드 상태에서 피격 시 응답 (Surface Type 무관, 단일 적용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Block")
	FImpactResponseData BlockedResponse;

	//가드 브레이크 시 응답
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|GuardBreak")
	FImpactResponseData GuardBreakResponse;

	//Surface Type별 일반 피격 응답
	//에디터에서 Physical Surface 이름("Flesh", "Metal" 등)으로 표시된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Normal")
	TMap<TEnumAsByte<EPhysicalSurface>, FImpactResponseData> NormalResponses;

	//NormalResponses에 매칭되는 항목이 없을 때 사용하는 기본 응답
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact|Normal")
	FImpactResponseData DefaultNormalResponse;

#pragma endregion

#pragma region "Public Functions"

	//가드 여부와 SurfaceType으로 응답 데이터를 반환한다.
	//bIsBlocked면 BlockedResponse, 아니면 NormalResponses 룩업 (없으면 DefaultNormalResponse).
	const FImpactResponseData& GetResponse(EPhysicalSurface SurfaceType, EDefenseResult InDefenseResult) const;

#pragma endregion
};
