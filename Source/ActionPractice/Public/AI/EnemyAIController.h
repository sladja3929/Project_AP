#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class AActionPracticeCharacter;
class UStateTreeAIComponent;
class UGASStateTreeAIComponent;
class AEnemyCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * Enemy AI가 추적하는 Target 정보
 */
USTRUCT()
struct ACTIONPRACTICE_API FCurrentTarget
{
	GENERATED_BODY()

	//Target Actor
	TWeakObjectPtr<AActor> Actor = nullptr;

	//Target까지의 거리
	float Distance = -1.0f;

	//Enemy의 정면 기준 Target과의 각도
	float AngleToTarget = 0.0f;

	void Reset()
	{
		Actor.Reset();
		Distance = -1.0f;
		AngleToTarget = 0.0f;
	}

	bool IsValid() const { return Actor.IsValid(); }
};

//전투 상태 변경 알림 (bInCombat, 전투 대상)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEnemyCombatStateChanged, bool /*bInCombat*/, AActor* /*Target*/);

/**
 * 적 AI 컨트롤러 (서버 전용)
 * 타깃 인지 상태와 전투 상태를 StateTree와 별개로 소유
 * - 인지: AIPerception 시야(시야각·SightRadius·가림)로 획득, 시야를 벗어나면 해제
 * - 전투: StateTree(Enter Combat 태스크) 또는 피격으로 진입. 시야와 무관하게 타깃을 유지하고, 타깃 사망·LoseSightRadius 초과 거리·자신의 사망/리셋으로만 해제
 */
UCLASS()
class ACTIONPRACTICE_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	FCurrentTarget CurrentTarget;

	//전투 진입·해제 시 브로드캐스트 (보스 조우 연출 등)
	FOnEnemyCombatStateChanged OnCombatStateChanged;

#pragma endregion

#pragma region "Public Functions"

	AEnemyAIController();

	FORCEINLINE UGASStateTreeAIComponent* GetStateTreeComponent() const { return GASStateTreeAIComponent; }
	FORCEINLINE AEnemyCharacter* GetEnemyCharacter() const { return EnemyCharacter.Get(); }
	FORCEINLINE AActor* GetCurrentTargetActor() const { return CurrentTarget.Actor.Get(); }
	FORCEINLINE const FCurrentTarget& GetCurrentTarget() const { return CurrentTarget; }
	FORCEINLINE bool IsInCombat() const { return bInCombat; }

	//전투 진입. 살아 있는 플레이어만 대상이며, 이미 전투 중이면 기존 대상 유지
	void EnterCombat(AActor* Target);

	//전투 해제. 시야 안에 살아 있는 플레이어가 있으면 그 플레이어를 인지한 상태로 복귀
	void ExitCombat();

	//전투·인지 상태 초기화 (리셋). 인지 기록도 지워 시야 안의 플레이어를 다시 감지하게 함
	void ResetAwareness();

	//사망 시 전투·인지 정리. 사망 연출은 사망 어빌리티 담당이라 전투 해제 알림 없음, 이후 인지 이벤트도 무시됨
	void ClearAwarenessOnDeath();

#pragma endregion

protected:
#pragma region "Protected Variables"

	//State Tree 에셋을 브레인으로 설정하고 실행하는 컴포넌트
	//5.6부터는 스키마와 컴포넌트가 일치해야 함(기본 컴포넌트는 커스텀 스키마 에셋이 에셋 피커에서 뜨지 않음, 버그일수도)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UGASStateTreeAIComponent> GASStateTreeAIComponent;

	//전투 해제 조건(타깃 사망·거리) 검사 주기 (초)
	UPROPERTY(EditDefaultsOnly, Category = "AI|Combat", meta = (ClampMin = "0.05"))
	float CombatReleaseCheckInterval = 0.25f;

#pragma endregion

#pragma region "Protected Functions"

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	//Perception 이벤트 핸들러
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AEnemyCharacter> EnemyCharacter;

	//전투 상태 여부 (true면 시야 변화로 타깃을 잃지 않음)
	bool bInCombat = false;

	FTimerHandle CombatReleaseCheckTimer;

#pragma endregion

#pragma region "Private Functions"

	//전투 해제 조건 검사 (타이머 콜백)
	void CheckCombatRelease();

	//현재 시야에 들어와 있는 살아 있는 플레이어 (없으면 nullptr)
	AActor* FindPerceivedPlayer() const;

	//플레이어이고 State.Dead 태그가 없는지
	static bool IsAlivePlayer(const AActor* Actor);

#pragma endregion
};