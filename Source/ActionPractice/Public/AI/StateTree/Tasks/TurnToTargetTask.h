#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TurnToTargetTask.generated.h"

class UEnemyAbilitySystemComponent;
class AEnemyAIController;
class UAnimMontage;

USTRUCT()
struct FTurnToTargetTaskInstanceData
{
	GENERATED_BODY()

	//Context에 바인딩된 ASC
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UEnemyAbilitySystemComponent> EnemyAbilitySystemComponent = nullptr;

	//Context에 바인딩된 AI 컨트롤러 (타깃·각도 조회용)
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

	//타깃이 왼쪽에 있을 때 재생할 제자리 회전 몽타주. TurnTarget MotionWarping ANS(Rotation Method = Scale) 필요
	//좌우 몽타주가 모두 없으면 걸으면서 회전
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> TurnLeftMontage = nullptr;

	//타깃이 오른쪽에 있을 때 재생할 제자리 회전 몽타주
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> TurnRightMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	//이 각도(절대값) 안으로 들어오면 회전 완료
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ExitAngle = 15.0f;

	//최대 회전 시간 (초). 넘으면 실패로 종료
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1"))
	float Timeout = 3.0f;

	//걸으며 회전할 때 이동 입력 방향을 정면에서 타깃 쪽으로 꺾는 최대 각도. 작을수록 큰 호를 그리며 회전
	UPROPERTY(EditAnywhere, Category = "Parameter|Walk", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float MaxInputAngle = 60.0f;

	//걸으며 회전할 때 이동 입력 크기 (현재 최대 이동 속도 대비 비율)
	UPROPERTY(EditAnywhere, Category = "Parameter|Walk", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WalkInputScale = 0.5f;

	//재생 중인 회전 몽타주 (nullptr이면 걸으며 회전)
	UPROPERTY()
	TObjectPtr<UAnimMontage> PlayingMontage = nullptr;

	//EnterState 이후 경과 시간
	float ElapsedTime = 0.0f;

	//이 태스크가 워프 타깃을 세팅했는지 여부 (ExitState 정리 대상 판단)
	bool bWarpTargetSet = false;
};

/**
 * 타깃을 정면에 둘 때까지 회전하는 Task
 * 행동 그룹이 시작 각도 조건(Target Angle)에 걸려 선택되지 않았을 때의 fallback 상태에서 사용하고, 완료 후 상위 상태를 재선택
 * - 회전 몽타주 지정 시: 타깃 방향(좌/우) 몽타주를 ASC로 재생하고 TurnTarget 워프로 회전량을 실제 각도에 맞춤 (발 스텝이 있어 미끄러짐 없음)
 * - 회전 몽타주 미지정 시: 정면에서 타깃 쪽으로 MaxInputAngle만큼 꺾은 방향으로 이동 입력 → bOrientRotationToMovement로 호를 그리며 회전
 *   (실제 속도가 생겨 ABP가 Idle 대신 Walk 재생. 이동 속도는 같은 상태의 GE 태스크로 지정)
 * 각도가 ExitAngle 안으로 들어오거나 몽타주가 끝나면 Succeeded, 타깃 상실·Timeout은 Failed
 */
USTRUCT(meta = (DisplayName = "Turn To Target", Category = "AI"))
struct ACTIONPRACTICE_API FTurnToTargetTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTurnToTargetTaskInstanceData;

	FTurnToTargetTask() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
