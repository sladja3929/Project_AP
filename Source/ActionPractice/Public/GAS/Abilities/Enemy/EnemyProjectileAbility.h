#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyAttackAbility.h"
#include "EnemyProjectileAbility.generated.h"

class AEnemyProjectile;
struct FEnemyProjectileConfig;

/***
 * 적 투사체 공격 어빌리티 (단일 몽타주 + 노티파이 기반 타이밍)
 *
 * 부모(EnemyAttackAbility)의 DA 조회·쿨다운·몽타주·AttackTarget 회전 워프(조준 회전)·리커버리 흐름을 그대로 쓰고
 * 투사체 생성·발사만 추가:
 * - Projectile Spawn 노티파이: 투사체 생성. bPresentBeforeGrab이면 오너 앞 등장 위치에 놓고, 아니면 바로 손 소켓에 부착
 * - Projectile Grab 노티파이: 등장해 있던 투사체를 손 소켓에 부착
 * - Projectile Launch 노티파이: 발사 시점 타깃 위치 + 이동 예측(LeadFactor)으로 포물선 속도 계산 후 발사
 * - 발사 후 데미지는 투사체가 적용하므로 어빌리티가 먼저 끝나도 판정 유지, 발사 전 취소면 투사체 제거
 * - 등장·잡기 연출은 투사체 BP의 단계 이벤트가 담당 (어빌리티는 단계만 진행)
 *
 * ComboSequence[0] = 투사체 몽타주 (본체 히트 판정 준비 생략)
 * ComboSequence[1+] = 후속 콤보 (부모 EnemyAttackAbility 흐름 복귀)
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyProjectileAbility : public UEnemyAttackAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UEnemyProjectileAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//===== 투사체 설정 캐싱 =====
	const FEnemyProjectileConfig* CachedProjectileConfig = nullptr;

	//생성 후 발사 전 투사체 (등장 또는 손에 든 상태, 발사하면 해제)
	UPROPERTY()
	TObjectPtr<AEnemyProjectile> PendingProjectile = nullptr;

	//===== 이벤트 태스크 =====
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitProjectileSpawnEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitProjectileGrabEventTask = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitProjectileLaunchEventTask = nullptr;

	//===== 태그 캐싱 =====
	FGameplayTag EventNotifyProjectileSpawnTag;
	FGameplayTag EventNotifyProjectileGrabTag;
	FGameplayTag EventNotifyProjectileLaunchTag;

#pragma endregion

#pragma region "Protected Functions"

	//===== 초기화 =====
	virtual void ActivateInitSettings() override;
	void CacheProjectileConfig();
	void CacheProjectileTags();

	//===== 히트 디텍션 =====
	//투사체 콤보(인덱스 0)는 본체 판정을 준비하지 않음 (판정은 투사체가 담당)
	virtual void SetHitDetectionConfig() override;

	//===== 투사체 =====
	void SpawnProjectile();
	void GrabProjectile();
	void LaunchProjectile();
	void DestroyPendingProjectile();

	//발사 시점 타깃 위치·이동 예측으로 포물선 발사 속도 계산. 타깃이 없으면 false
	bool CalculateLaunchVelocity(const FVector& StartLocation, float GravityZ, FVector& OutVelocity) const;

	//===== 이벤트 핸들러 =====
	UFUNCTION()
	void OnEventProjectileSpawn(FGameplayEventData Payload);

	UFUNCTION()
	void OnEventProjectileGrab(FGameplayEventData Payload);

	UFUNCTION()
	void OnEventProjectileLaunch(FGameplayEventData Payload);

#pragma endregion
};
