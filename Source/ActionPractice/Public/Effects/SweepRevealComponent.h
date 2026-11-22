#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SweepRevealComponent.generated.h"

class UMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UNiagaraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSweepRevealFinished, bool, bRevealed);

/**
 * 메시를 높이 방향으로 쓸어내리듯 드러내거나 감추는 로컬 연출 컴포넌트
 *
 * -메시 머티리얼은 MF_SweepReveal을 쓰는 머티리얼이어야 함 (RevealAlpha·RevealMinZ·RevealMaxZ 파라미터)
 * -드러남: RevealAlpha 0 → 1 (아래 → 위), 감춤: 1 → 0 (위 → 아래)
 * -경계 이펙트(선택)는 매 틱 경계 높이로 이동하며, User.EdgeRadius에 메시 수평 반경을 넘김
 * -복제 없는 로컬 연출. 각 머신에서 호출하며 데디 서버에서는 PlaySweepOnMesh가 아무것도 하지 않음
 * -액터당 컴포넌트 1개·스윕 1개. 같은 액터의 다른 메시로 다시 호출하면 진행 중인 스윕과 설정을 대체함
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API USweepRevealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(BlueprintAssignable, Category = "Sweep Reveal")
	FOnSweepRevealFinished OnSweepFinished;

	//경계를 따라가는 이펙트 (없으면 머티리얼 스윕만)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweep Reveal")
	TObjectPtr<UNiagaraSystem> EdgeEffect = nullptr;

	//경계 이펙트 고리 반경 = 메시 수평 바운드 반경 × 이 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweep Reveal", meta = (ClampMin = "0.0"))
	float EdgeRadiusScale = 0.8f;

	//감춤이 끝나면 소유 액터 제거
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweep Reveal")
	bool bDestroyOwnerWhenHidden = false;

#pragma endregion

#pragma region "Public Functions"

	USweepRevealComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//Mesh를 Duration 동안 드러내거나(bReveal) 감춤. 진행 중이면 처음부터 다시 시작
	//StartDelay 동안은 시작 상태(드러냄이면 완전히 숨김)로 대기 후 진행, 경계 이펙트도 그때 시작
	UFUNCTION(BlueprintCallable, Category = "Sweep Reveal")
	void PlaySweep(UMeshComponent* Mesh, bool bReveal, float Duration, float StartDelay = 0.0f);

	//Mesh 소유 액터의 스윕 컴포넌트(없으면 추가)로 스윕 실행
	UFUNCTION(BlueprintCallable, Category = "Sweep Reveal")
	static USweepRevealComponent* PlaySweepOnMesh(UMeshComponent* Mesh, bool bReveal, float Duration, UNiagaraSystem* InEdgeEffect, bool bInDestroyOwnerWhenHidden, float StartDelay = 0.0f);

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<UMeshComponent> TargetMesh;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TargetMaterials;

	TWeakObjectPtr<UNiagaraComponent> EdgeComponent;

	float SweepDuration = 0.5f;

	//음수면 시작 지연 중
	float SweepElapsed = 0.0f;
	bool bSweepReveal = true;

	//시작 지연이 끝나면 경계 이펙트 생성
	bool bEdgeEffectPending = false;

	static const FName ParamRevealAlpha;
	static const FName ParamRevealMinZ;
	static const FName ParamRevealMaxZ;
	static const FName ParamEdgeRadius;

#pragma endregion

#pragma region "Protected Functions"

	//진행값을 머티리얼·경계 이펙트에 반영 (바운드는 매번 다시 읽어 메시 이동을 따라감)
	void ApplyAlpha(float Alpha);

	void FinishSweep();
	void StartEdgeEffect();
	void StopEdgeEffect();

#pragma endregion
};
