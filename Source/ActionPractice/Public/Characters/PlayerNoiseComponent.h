#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "PlayerNoiseComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

/**
 * 플레이어가 내는 소리를 AI 청각(AISense_Hearing)에 전달하는 컴포넌트 (서버 전용)
 * - 이동: 지면 이동 속도에 비례한 크기로 주기 발생 (발소리 노티파이가 일부 애니메이션에만 있어 속도 기준으로 통일)
 * - 전투: 플레이어가 주거나 받은 대미지 GE 적용 시 발생 (데미지 파이프라인과 분리, ASC GE 적용 델리게이트 사용)
 * - State.Undetectable·State.Dead 보유 중에는 발생하지 않음 (시야 차단과 같은 기준)
 * 소리 크기는 Loudness 배율이며, 실제 들리는 거리는 적 HearingRange × Loudness
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UPlayerNoiseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UPlayerNoiseComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//이동 소리 발생 주기 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Noise|Movement", meta = (ClampMin = "0.05"))
	float MovementNoiseInterval = 0.4f;

	//이동 소리가 나기 시작하는 지면 수평 속도 (이하는 무음)
	UPROPERTY(EditDefaultsOnly, Category = "Noise|Movement", meta = (ClampMin = "0.0"))
	float MinMovementNoiseSpeed = 150.0f;

	//이동 소리가 최대 크기가 되는 지면 수평 속도 (스프린트 기준)
	UPROPERTY(EditDefaultsOnly, Category = "Noise|Movement", meta = (ClampMin = "1.0"))
	float LoudestMovementSpeed = 600.0f;

	//MinMovementNoiseSpeed에서의 소리 크기
	UPROPERTY(EditDefaultsOnly, Category = "Noise|Movement", meta = (ClampMin = "0.0"))
	float MinMovementLoudness = 0.3f;

	//LoudestMovementSpeed 이상에서의 소리 크기
	UPROPERTY(EditDefaultsOnly, Category = "Noise|Movement", meta = (ClampMin = "0.0"))
	float MaxMovementLoudness = 1.0f;

	//대미지를 주거나 받았을 때의 소리 크기
	UPROPERTY(EditDefaultsOnly, Category = "Noise|Combat", meta = (ClampMin = "0.0"))
	float CombatLoudness = 1.5f;

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle AppliedToTargetHandle;
	FDelegateHandle AppliedToSelfHandle;

	FTimerHandle MovementNoiseTimer;

	FGameplayTag DamageTag;
	FGameplayTag UndetectableTag;
	FGameplayTag DeadTag;

#pragma endregion

#pragma region "Private Functions"

	//이동 소리 발생 (타이머 콜백)
	void EmitMovementNoise();

	//플레이어 ASC가 다른 대상에 GE 적용 (공격 적중)
	void OnEffectAppliedToTarget(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

	//플레이어 ASC에 GE 적용 (피격·가드)
	void OnEffectAppliedToSelf(UAbilitySystemComponent* SourceASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

	//대미지 GE 여부 (IncomingDamage SetByCaller 보유)
	bool IsDamageSpec(const FGameplayEffectSpec& Spec) const;

	//소리를 낼 수 있는 상태인지 (Undetectable·사망 아님)
	bool CanMakeNoise() const;

	void ReportNoise(const FVector& Location, float Loudness, FName Tag) const;

#pragma endregion
};
