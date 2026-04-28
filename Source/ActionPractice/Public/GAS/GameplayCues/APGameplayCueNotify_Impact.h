#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayCues/APGameplayCueNotify_Instant.h"
#include "GAS/GameplayCues/ImpactResponseDataAsset.h"
#include "Items/AttackData.h"
#include "APGameplayCueNotify_Impact.generated.h"

class UNiagaraSystem;
class USoundBase;
struct FActionPracticeGameplayEffectContext;

//피격 시 Location/Normal 기반으로 이펙트를 스폰하는 Impact 전용 Instant 큐.
//FGameplayCueParameters에서 피격 위치와 법선을 읽고,
//ActionPracticeGameplayEffectContext에서 공격 속성(DamageType 등)을 읽어
//서브클래스에서 속성별 이펙트 분기가 가능하도록 확장 포인트를 제공한다.
UCLASS(Blueprintable)
class ACTIONPRACTICE_API UAPGameplayCueNotify_Impact : public UAPGameplayCueNotify_Instant
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UAPGameplayCueNotify_Impact();

#pragma endregion

protected:
#pragma region "Protected Variables"

	//true면 Parameters.Normal 방향으로 이펙트를 회전한다 (피격 면 기반 스파크 방향)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Direction")
	bool bAlignToNormal = true;

	// ===== DataAsset 룩업 =====

	//(Surface Type × 가드 여부) 조합별 이펙트/사운드 매핑 데이터
	//설정되어 있으면 ResolveEffectByContext에서 DA 룩업 결과를 우선 사용한다.
	//nullptr이면 부모의 NiagaraEffect/InstantSound 프로퍼티를 폴백으로 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|ImpactResponse")
	TObjectPtr<UImpactResponseDataAsset> ImpactResponseDA = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	//부모의 GetSpawnTransform을 오버라이드하여 Parameters의 Location/Normal을 사용한다.
	virtual void GetSpawnTransform_Implementation(AActor* TargetActor, const FGameplayCueParameters& Parameters, FVector& OutLocation, FRotator& OutRotation) const override;

	// ===== 속성 기반 분기 확장 포인트 (추후 구현) =====

	//Context에서 공격 속성(DamageType 등)을 읽어 이펙트/사운드를 결정한다.
	//기본 구현은 아무것도 하지 않는다. 서브클래스에서 오버라이드하여
	//목재/철/피부 등의 재질 × 타격/참격 등의 유형 조합에 따라
	//NiagaraEffect와 InstantSound를 교체하는 용도로 사용한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Cue")
	void ResolveEffectByContext(const FGameplayCueParameters& Parameters, UNiagaraSystem*& OutNiagara, USoundBase*& OutSound) const;
	virtual void ResolveEffectByContext_Implementation(const FGameplayCueParameters& Parameters, UNiagaraSystem*& OutNiagara, USoundBase*& OutSound) const;

	//OnExecute를 오버라이드하여 ResolveEffectByContext 호출 후 스폰한다.
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
