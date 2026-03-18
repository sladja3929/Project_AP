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
struct FAIStimulus;

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

#pragma endregion

#pragma region "Public Functions"

	AEnemyCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Hit Detection Interface =====
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const override;

	FORCEINLINE UEnemyAttributeSet* GetAttributeSet() const { return Cast<UEnemyAttributeSet>(AttributeSet); }
	FORCEINLINE AEnemyAIController* GetEnemyAIController() const { return Cast<AEnemyAIController>(GetController()); }

	const UEnemyDataAsset* GetEnemyData() const { return EnemyData.Get(); }

	void RotateToTarget(const AActor* TargetActor, float RotateTime);

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

	//HP바 위젯 클래스 (BP에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UEnemyHealthBarWidget> EnemyHealthBarWidgetClass;

#pragma endregion

#pragma region "Protected Functions"

	// ===== GAS =====
	virtual void CreateAbilitySystemComponent() override;
	virtual void CreateAttributeSet() override;

	// ===== Enemy Reset Helpers =====
	void ApplyEnemyResetEffect();
	void RestartEnemyAI();

	// ===== Perception =====
	UFUNCTION()
	virtual void OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus);

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AActionPracticeCharacter> DetectedPlayer;

	//초기 스폰 Transform 캐시
	FTransform InitialTransform;

	//락온 중인지 여부 (LockOnComponent에서 설정)
	bool bLockedOnByPlayer = false;

	//HP바 자동 숨김 타이머
	FTimerHandle HealthBarVisibilityTimer;

#pragma endregion

#pragma region "Private Functions"

	//타이머 콜백 — 락온 중이 아니면 HP바 숨김
	void OnHealthBarTimerExpired();

#pragma endregion
};
