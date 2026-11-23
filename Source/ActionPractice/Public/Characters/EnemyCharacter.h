#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "AI/EnemyAIController.h"
#include "EnemyCharacter.generated.h"

class AActionPracticeCharacter;
class UEnemyAttackComponent;
class UEnemyDataAsset;
class UGameplayEffect;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class AEnemyWanderArea;

UCLASS()
class ACTIONPRACTICE_API AEnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FName EnemyName = NAME_None;

	//머리 위 HP바 표시 지속 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	float HealthBarVisibilityDuration = 5.0f;

	//ST Property Binding용 스폰 위치 캐시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	FVector SpawnLocation = FVector::ZeroVector;

	//Idle 배회 구역 (레벨 배치 인스턴스별 지정). 무리는 같은 구역을 공유, 없으면 스폰 위치 반경 배회
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<AEnemyWanderArea> WanderArea;

#pragma endregion

#pragma region "Public Functions"

	AEnemyCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	//AI 빙의 시 GAS 초기화 (컨트롤러 OnPossess가 StateTree를 시작하기 전에 ASC를 준비)와 전투 상태 델리게이트 바인딩
	virtual void PossessedBy(AController* NewController) override;

	//빙의 해제 시 전투 상태 델리게이트 해제
	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Hit Detection Interface =====
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const override;

	FORCEINLINE UEnemyAttributeSet* GetAttributeSet() const { return Cast<UEnemyAttributeSet>(AttributeSet); }
	FORCEINLINE AEnemyAIController* GetEnemyAIController() const { return Cast<AEnemyAIController>(GetController()); }

	const UEnemyDataAsset* GetEnemyData() const { return EnemyData.Get(); }
	FORCEINLINE AEnemyWanderArea* GetWanderArea() const { return WanderArea; }

	//전투 중인 대상 (서버 EnemyAIController 전투 상태를 복제, 머리 바라보기 등 연출용)
	FORCEINLINE AActor* GetCombatTarget() const { return CombatTarget; }

	void RotateToTarget(const AActor* TargetActor, float RotateTime);

	//타깃이 이 적의 전투 구역 안에 있는지 (안에 있는 동안은 거리로 전투 해제하지 않음). 기본은 구역 없음, 보스는 보스룸으로 판정
	virtual bool IsTargetInCombatArea(const AActor* Target) const { return false; }

	// ===== Enemy Reset =====
	void CacheInitialEnemyState();

	//GameMode에서 호출 — 자식에서 override 가능
	virtual void ResetEnemy();

	//머리 위 HP바 표시 — 피격 시, 락온 시 호출
	virtual void ShowEnemyHealthBar();

	//머리 위 HP바 숨김
	virtual void HideEnemyHealthBar();

	//락온 상태 설정 (LockOnComponent에서 호출)
	void SetLockedOnByPlayer(bool bLocked);

	//사망 시 모든 클라이언트에서 이 적을 타깃으로 한 락온 해제
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReleaseLockOn();

	//사망 포즈 고정 — ServerOnly 어빌리티는 bPauseAnims가 복제되지 않으므로 Multicast로 전파
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FreezeAnimPose();

	//시체 충돌 전환 (서버 전용) — 사망 시 true로 캡슐의 Pawn 충돌을 해제해 이동을 막지 않게 하고, 리셋 시 false로 복구
	void SetCorpseCollision(bool bCorpse);

	// ===== Replication =====
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TObjectPtr<UEnemyDataAsset> EnemyData;

	UPROPERTY(EditDefaultsOnly, Category = "Reset")
	TSubclassOf<UGameplayEffect> EnemyResetRecoveryEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UEnemyAttackComponent> EnemyAttackComponent;

	//머리 위 HP바 위젯 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UWidgetComponent> EnemyHealthBarWidgetComponent;

	//전투 대상 (서버 OnCombatStateChanged에서 설정, 시뮬레이티드 프록시에도 복제)
	UPROPERTY(Replicated, Transient)
	TObjectPtr<AActor> CombatTarget;

#pragma endregion

#pragma region "Protected Functions"

	// ===== GAS =====
	virtual void CreateAbilitySystemComponent() override;
	virtual void CreateAttributeSet() override;

	// ===== Enemy Reset Helpers =====
	void ApplyEnemyResetEffect();
	void RestartEnemyAI();

	// ===== Combat State =====
	//EnemyAIController의 전투 진입·해제 알림 (서버). 인지(시야) 변화가 아닌 전투 상태 기준
	virtual void OnCombatStateChanged(bool bInCombat, AActor* Target);

#pragma endregion

private:
#pragma region "Private Variables"

	//초기 스폰 Transform 캐시
	FTransform InitialTransform;

	//락온 중인지 여부 (LockOnComponent에서 설정)
	bool bLockedOnByPlayer = false;

	//HP바 자동 숨김 타이머
	FTimerHandle HealthBarVisibilityTimer;

	//시체 상태 — 충돌 응답은 복제되지 않으므로 상태를 복제해 각 머신에서 캡슐 충돌 적용 (늦게 relevant된 클라이언트 포함)
	UPROPERTY(ReplicatedUsing = OnRep_IsCorpse, Transient)
	bool bIsCorpse = false;

	//복구용 캡슐 Pawn 채널 원래 응답 (BeginPlay 캐시, BP 오버라이드 반영)
	TEnumAsByte<ECollisionResponse> DefaultCapsulePawnResponse = ECR_Block;

#pragma endregion

#pragma region "Private Functions"

	//타이머 콜백 — 락온 중이 아니면 HP바 숨김
	void OnHealthBarTimerExpired();

	UFUNCTION()
	void OnRep_IsCorpse();

	//bIsCorpse에 맞춰 캡슐 Pawn 채널 응답 적용
	void ApplyCorpseCollision();

#pragma endregion
};
