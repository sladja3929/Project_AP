#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "APGameplayCueNotify_Instant.generated.h"

class UNiagaraSystem;
class USoundBase;

//Instant GE 연동 Gameplay Cue 범용 베이스 클래스
//GE 실행 시 원샷 Niagara 이펙트와 사운드를 한 번 재생하고 자동으로 정리된다.
//CDO에서 실행되므로 멤버 상태를 보관하지 않는다.
//Duration 큐(AAPGameplayCueNotify_Duration)와 달리 OnExecute 콜백만 사용한다.
UCLASS(Blueprintable)
class ACTIONPRACTICE_API UAPGameplayCueNotify_Instant : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UAPGameplayCueNotify_Instant();

#pragma endregion

protected:
#pragma region "Protected Variables"

	// ===== Niagara 설정 =====

	//재생할 Niagara System 에셋 (원샷, 자동 소멸)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TObjectPtr<UNiagaraSystem> NiagaraEffect = nullptr;

	//이펙트 스케일
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FVector EffectScale = FVector::OneVector;

	// ===== 위치 설정 =====

	//부착 대상 소켓 이름 (기본값: spine_02)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Location")
	FName AttachSocketName = FName("spine_02");

	//소켓 기준 로컬 위치 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Location")
	FVector LocationOffset = FVector::ZeroVector;

	//소켓 기준 로컬 회전 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Location")
	FRotator RotationOffset = FRotator::ZeroRotator;

	//false면 소켓 대신 액터 루트 위치에 스폰 (광역 이펙트용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Location")
	bool bAttachToSocket = true;

	// ===== 사운드 설정 =====

	//재생할 원샷 사운드 (nullptr이면 무시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|SFX")
	TObjectPtr<USoundBase> InstantSound = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	// ===== GameplayCueNotify_Static 오버라이드 =====

	//GE 실행 시 호출 — 원샷 Niagara/사운드 재생
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	// ===== 서브클래스 확장 포인트 =====

	//이펙트 스폰 위치와 회전을 결정한다.
	//기본 구현은 타겟 액터의 소켓 위치 + 오프셋을 사용한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Cue")
	void GetSpawnTransform(AActor* TargetActor, const FGameplayCueParameters& Parameters, FVector& OutLocation, FRotator& OutRotation) const;
	virtual void GetSpawnTransform_Implementation(AActor* TargetActor, const FGameplayCueParameters& Parameters, FVector& OutLocation, FRotator& OutRotation) const;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
