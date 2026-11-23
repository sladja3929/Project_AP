#include "AI/StateTree/Tasks/EnterCombatTask.h"
#include "AI/EnemyAIController.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnterCombatTask, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnterCombatTask, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

FEnterCombatTask::FEnterCombatTask()
{
	//완료 판정에서 제외하면 이 태스크만 가진 상태(Combat)가 진입 즉시 완료 처리되므로 기본값(포함) 유지
	//EnterState 후 계속 Running이라 상태 완료는 같은 상태의 다른 태스크나 자식 상태가 결정
	bShouldCallTick = false;
}

EStateTreeRunStatus FEnterCombatTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyAIController* AIController = InstanceData.EnemyAIController;

	if (!AIController)
	{
		DEBUG_LOG(TEXT("EnterState: AIController is null"));
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = AIController->GetCurrentTargetActor();

	//타깃이 없어도 상태 흐름에 영향 주지 않도록 Running 유지 (진입 조건과 한 틱 어긋날 수 있음)
	if (!Target)
	{
		DEBUG_LOG(TEXT("EnterState: No target"));
		return EStateTreeRunStatus::Running;
	}

	//이미 전투 중이면 무시됨
	AIController->EnterCombat(Target);

	return EStateTreeRunStatus::Running;
}
