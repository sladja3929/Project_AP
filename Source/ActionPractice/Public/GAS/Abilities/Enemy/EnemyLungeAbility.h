#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAttackAbility.h"
#include "EnemyLungeAbility.generated.h"

class UCharacterMovementComponent;
struct FEnemyLungeConfig;

/***
 * 적 돌진 공격 어빌리티 (단일 몽타주 + ANS 기반 타이밍)
 *
 * 몽타주 내 ANS 배치로 모든 타이밍을 제어:
 * - TrackingTarget ANS: 타겟 추적 (회전 + 목적지 캐싱). 이동 중에도 겹칠 수 있음
 * - Lunging ANS: 코드 이동 구간. ANS 지속시간 = 이동 시간
 * - HitDetection ANS: 착지 공격 판정
 * - ActionRecovery 커브: 경직 + 콤보 전이 트리거
 *
 * ComboSequence[0] = Lunge 통합 몽타주
 * ComboSequence[1+] = 후속 콤보 (부모 EnemyAttackAbility 흐름 복귀)
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyLungeAbility : public UEnemyAttackAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UEnemyLungeAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	// ===== Lunge 설정 캐싱 =====
	const FEnemyLungeConfig* CachedLungeConfig = nullptr;

	// ===== 이동 =====

	//TrackingTarget ANS에서 갱신되는 타겟 위치
	FVector CachedDestination = FVector::ZeroVector;

	//lunge 시작 위치 (lerp 출발점 보존용 — 재타게팅 시 source 재생성에 사용)
	FVector OriginalLungeStartLocation = FVector::ZeroVector;

	//lunge 시작 월드시간 (Elapsed 계산용)
	float LungeStartWorldTime = 0.0f;

	//lunge 원래 Duration (재타게팅 시 보존)
	float LungeOriginalDuration = 0.0f;

	//활성 RootMotionSource의 LocalID (0 == ERootMotionSourceID::Invalid)
	uint16 CurrentLungeSourceID = 0;

	//마지막으로 등록한 source의 StartLocation (재타게팅 시 OldLerpGround 계산용)
	FVector LastSourceStartLocation = FVector::ZeroVector;

	//마지막으로 등록한 source의 TargetLocation (재타게팅 시 OldLerpGround 계산용)
	FVector LastSourceTargetLocation = FVector::ZeroVector;

	// ===== 이벤트 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitTrackingTargetEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitLungeStartEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitLungeEndEventTask = nullptr;

	// ===== 태그 캐싱 =====
	FGameplayTag EventNotifyTrackingTargetTag;
	FGameplayTag EventNotifyLungeStartTag;
	FGameplayTag EventNotifyLungeEndTag;

#pragma endregion

#pragma region "Protected Functions"

	// ===== 초기화 =====
	virtual void ActivateInitSettings() override;
	void CacheLungeConfig();
	void CacheLungeTags();

	// ===== 이동 =====
	void StartLungeMovement(float Duration);
	void StopLungeMovement();
	bool IsLungeActive() const;

	//새 RootMotionSource 인스턴스를 만들어 CMC에 등록. LocalID 반환 (0 == 실패)
	uint16 AddLungeRootMotionSource(UCharacterMovementComponent* CMC,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		float Duration,
		float TimeOffset);

	// ===== 이벤트 핸들러 =====
	UFUNCTION()
	void OnEventTrackingTarget(FGameplayEventData Payload);

	UFUNCTION()
	void OnEventLungeStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnEventLungeEnd(FGameplayEventData Payload);

	// ===== 오버라이드 =====

	//Lunge 콤보(인덱스 0)는 CheckCondition 없이 강제 진행
	//후속 콤보(인덱스 1+)는 부모의 일반 흐름 복귀
	virtual void OnActionRecoveryEnd() override;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
