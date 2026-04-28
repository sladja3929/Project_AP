#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "APGameplayCueNotify_Duration.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UAudioComponent;
class USoundBase;

//Duration/Infinite GE 연동 Gameplay Cue 범용 베이스 클래스
//GE 적용 시 Niagara 이펙트와 사운드를 대상 소켓에 부착하고, GE 제거 시 정리한다.
//BP 자식 클래스에서 프로퍼티만 세팅하면 새 버프 비주얼을 코드 수정 없이 추가할 수 있다.
UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class ACTIONPRACTICE_API AAPGameplayCueNotify_Duration : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	AAPGameplayCueNotify_Duration();

#pragma endregion

protected:
#pragma region "Protected Variables"

	// ===== Niagara 설정 =====

	//부착할 Niagara System 에셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TObjectPtr<UNiagaraSystem> NiagaraEffect = nullptr;

	//부착 대상 소켓 이름 (기본값: spine_02)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FName AttachSocketName = FName("spine_02");

	//소켓 기준 로컬 위치 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FVector LocationOffset = FVector::ZeroVector;

	//소켓 기준 로컬 회전 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FRotator RotationOffset = FRotator::ZeroRotator;

	//이펙트 스케일
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FVector EffectScale = FVector::OneVector;

	//false면 소켓 대신 액터 루트 컴포넌트에 부착 (광역 이펙트용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	bool bAttachToSocket = true;

	// ===== 사운드 설정 =====

	//GE 지속 중 재생되는 루핑 사운드 (nullptr이면 무시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|SFX")
	TObjectPtr<USoundBase> LoopingSound = nullptr;

	//GE 적용 시 재생되는 원샷 사운드 (nullptr이면 무시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|SFX")
	TObjectPtr<USoundBase> OneShotActivateSound = nullptr;

	//GE 제거 시 재생되는 원샷 사운드 (nullptr이면 무시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|SFX")
	TObjectPtr<USoundBase> OneShotRemoveSound = nullptr;

	// ===== 런타임 참조 =====

	//스폰된 Niagara 컴포넌트 (OnRemove에서 정리)
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> SpawnedNiagara = nullptr;

	//스폰된 루핑 Audio 컴포넌트 (OnRemove에서 정리)
	UPROPERTY()
	TObjectPtr<UAudioComponent> SpawnedAudio = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	// ===== GameplayCueNotify_Actor 오버라이드 =====

	//GE가 처음 적용될 때 호출 — Niagara/사운드 스폰
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	//GE가 이미 적용 중인 상태에서 재실행될 때 호출 — 중복 스폰 방지
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	//GE가 제거될 때 호출 — Niagara/사운드 정리
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	// ===== 서브클래스 확장 포인트 =====

	//부착 대상 컴포넌트를 결정한다. 기본 구현은 타겟 액터의 SkeletalMeshComponent를 반환한다.
	//특정 컴포넌트에 부착해야 하는 경우 오버라이드한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Cue")
	USceneComponent* GetAttachTarget(AActor* TargetActor) const;
	virtual USceneComponent* GetAttachTarget_Implementation(AActor* TargetActor) const;

	//Niagara 스폰 직후 호출된다. 파라미터 주입이나 머티리얼 동적 변경에 사용한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Cue")
	void OnEffectSpawned(UNiagaraComponent* NiagaraComp);
	virtual void OnEffectSpawned_Implementation(UNiagaraComponent* NiagaraComp);

	//Niagara 제거 직전 호출된다. 페이드아웃 등 정리 로직에 사용한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Cue")
	void OnEffectRemoved(UNiagaraComponent* NiagaraComp);
	virtual void OnEffectRemoved_Implementation(UNiagaraComponent* NiagaraComp);

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	//Niagara 컴포넌트를 생성하고 대상에 부착한다.
	void SpawnNiagaraEffect(AActor* TargetActor);

	//루핑 사운드를 생성하고 대상에 부착한다.
	void SpawnLoopingSound(AActor* TargetActor);

	//원샷 사운드를 재생한다. AttachTarget이 있으면 해당 위치에서 재생한다.
	void PlayOneShotSound(AActor* TargetActor, USoundBase* Sound);

#pragma endregion
};
