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
class UEnemyPack;

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
 * - 인지: AIPerception 시야(시야각·SightRadius·가림)·청각(일반 적만, HearingRange × 소리 크기)으로 획득. 두 감각 모두에서 사라지면 해제 (청각은 HearingMaxAge 뒤 만료). 보스는 청각 대신 보스룸 진입(PerceiveTarget)으로 즉시 인지
 * - 전투: StateTree(Enter Combat 태스크) 또는 피격으로 진입. 시야와 무관하게 타깃을 유지하고, 타깃 사망·LoseSightRadius 초과 거리(타깃이 보스룸 등 전투 구역 안이면 제외)·자신의 사망/리셋으로만 해제
 * - 레시: 전투 중 스폰 위치에서 LeashDistance보다 멀어지면 타깃을 잃고 StateTree를 멈춘 채 스폰 위치로 걸어서 귀환
 *   귀환 중 피격·보스룸 진입은 즉시, 시야·청각은 레시 안쪽 여유 거리(LeashReacquireMargin) 안에서 다시 인지하고 StateTree 재시작. 도착하면 StateTree 재시작
 * - 무리: 무리 전투를 쓰는 적은 전투 진입 시 UEnemyPackSubsystem으로 무리를 소집. 무리 소속이면 거리 해제는 구성원 전원이 해제 거리 밖일 때만
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
	FORCEINLINE bool IsReturningHome() const { return bReturningHome; }
	FORCEINLINE UEnemyPack* GetPack() const { return Pack.Get(); }

	//공격권 보유 여부. 무리에 속하지 않으면 항상 true (혼자 싸우는 적은 늘 공격자)
	bool IsPackAttacker() const;

	//무리 소속 설정 (UEnemyPackSubsystem 전용)
	void SetPack(UEnemyPack* InPack);

	//감각 외 경로(보스룸 진입 등)로 타깃 인지. 전투 중·이미 인지한 타깃이 있으면 무시
	//살아 있고 State.Undetectable이 아닌 플레이어만 대상이며, 해제는 시야·전투 해제 규칙을 그대로 따름
	void PerceiveTarget(AActor* Target);

	//전투 진입. 살아 있는 플레이어만 대상이며, 이미 전투 중이면 기존 대상 유지
	void EnterCombat(AActor* Target);

	//전투 해제. 시야·청각으로 인지 중인 살아 있는 플레이어가 있으면 그 플레이어를 인지한 상태로 복귀
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

	//청각 기준 거리 (cm). 실제 들리는 거리는 이 값 × 소리 크기(Loudness). 보스는 청각을 쓰지 않음
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float HearingRange = 1500.0f;

	//들은 소리를 기억하는 시간 (초). 지나면 만료되어 시야에도 없으면 인지 해제 (0이면 만료되지 않으므로 금지)
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.1", Units = "Seconds"))
	float HearingMaxAge = 5.0f;

	//전투 중 스폰 위치에서 이 수평 거리보다 멀어지면 타깃을 잃고 스폰 위치로 귀환 (0이면 사용 안 함). 타깃이 전투 구역(보스룸) 안이면 제외
	UPROPERTY(EditDefaultsOnly, Category = "AI|Leash", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float LeashDistance = 3000.0f;

	//귀환 중 시야·청각으로 다시 인지하려면 스폰 위치에서 (LeashDistance - 이 값) 안으로 들어와 있어야 함 (레시 경계에서 추격·귀환이 반복되는 것 방지). 피격은 거리와 무관하게 즉시 재교전
	UPROPERTY(EditDefaultsOnly, Category = "AI|Leash", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float LeashReacquireMargin = 500.0f;

	//귀환 중 피격·보스룸 진입으로 재교전한 뒤 레시 판정을 미루는 시간 (초). 레시 밖에서 재교전 직후 바로 다시 귀환하는 반복 방지
	UPROPERTY(EditDefaultsOnly, Category = "AI|Leash", meta = (ClampMin = "0.0", Units = "Seconds"))
	float LeashReengageGrace = 5.0f;

	//귀환 완료로 보는 스폰 위치와의 수평 거리
	UPROPERTY(EditDefaultsOnly, Category = "AI|Leash", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float ReturnAcceptanceRadius = 150.0f;

	//귀환 최대 시간 (초). 경로가 막히거나 늦어져도 이 시간이 지나면 귀환을 끝내고 StateTree 재시작
	UPROPERTY(EditDefaultsOnly, Category = "AI|Leash", meta = (ClampMin = "1.0", Units = "Seconds"))
	float ReturnHomeTimeout = 15.0f;

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

	//교전을 통지한 플레이어 (플레이어 전투 상태 State.InCombat 집계용, CurrentTarget과 별도로 해제 통지 대상 보관)
	TWeakObjectPtr<AActor> EngagedTarget;

	FTimerHandle CombatReleaseCheckTimer;

	//소속 무리 (UEnemyPackSubsystem 소유, 전투 중에만 존재)
	TWeakObjectPtr<UEnemyPack> Pack;

	//스폰 위치로 귀환 중 (StateTree 정지. 인지 이벤트 대신 귀환 타이머가 재인지 판정, 피격·보스룸 진입은 즉시 재교전)
	bool bReturningHome = false;

	FTimerHandle ReturnHomeCheckTimer;

	double ReturnHomeStartTime = 0.0;

	//이 시각 전까지 레시 판정 보류 (귀환 중 재교전 유예)
	double LeashSuspendedUntil = 0.0;

#pragma endregion

#pragma region "Private Functions"

	//전투 해제 조건 검사 (타이머 콜백)
	void CheckCombatRelease();

	//교전 대상 변경 — 이전 대상에 이탈, 새 대상에 참여 통지 (플레이어 ASC가 State.InCombat 집계). nullptr이면 이탈만
	void SetEngagedTarget(AActor* NewTarget);

	//소속 무리 탈퇴
	void LeavePack();

	//레시 거리 초과 → 전투·타깃 해제 후 StateTree를 멈추고 스폰 위치로 걸어감
	void StartReturnHome();

	//귀환 중 재인지(레시 안쪽 여유 거리 안에서 시야·청각) 및 도착·이동 종료·시간 초과 검사 (타이머 콜백)
	void CheckReturnHome();

	//귀환 종료. bRestartLogic이면 StateTree 재시작 (재인지·도착), 아니면 정지 상태 유지 (리셋·사망·빙의 해제는 호출 측이 처리)
	void StopReturnHome(bool bRestartLogic);

	//귀환 중 피격·보스룸 진입으로 재교전 → 레시 유예 후 귀환 종료·StateTree 재시작
	void ReengageFromReturnHome();

	//스폰 위치로 이동 요청 (Failed: 경로 없음, AlreadyAtGoal: 이미 도착)
	EPathFollowingRequestResult::Type RequestMoveHome();

	//청각 감각 추가 (일반 적만). BP의 SensesConfig 직렬화에 덮이지 않도록 생성자가 아닌 빙의 시점에 런타임 구성
	void ConfigureHearing();

	//현재 시야·청각으로 인지 중인 살아 있는 플레이어 (없으면 nullptr)
	AActor* FindPerceivedPlayer() const;

	//플레이어이고 State.Dead 태그가 없는지
	static bool IsAlivePlayer(const AActor* Actor);

	//조종 중인 적의 사망 처리 여부 (사망한 적은 인지하지 않음)
	bool IsOwnerDeathHandled() const;

#pragma endregion
};