#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PlayMontageTask.generated.h"

class UEnemyAbilitySystemComponent;
class AEnemyAIController;
class UAnimMontage;

/**
 * Task Instance Data
 * Context: 스키마에서 설정한 Context에서 자동으로 같은 자료형을 찾아 바인딩
 * Parameter: 에디터에서 설정하는 입력 값 (직접 입력 or 다른 노드의 Output 연결)
 */

USTRUCT()
struct FPlayMontageTaskInstanceData
{
    GENERATED_BODY()

    //Context에 바인딩된 ASC
    UPROPERTY(EditAnywhere, Category = "Context")
    TObjectPtr<UEnemyAbilitySystemComponent> EnemyAbilitySystemComponent = nullptr;

    //재생할 몽타주 (루프는 몽타주 섹션의 Next Section 연결로 구성)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    float PlayRate = 1.0f;

    //시작 섹션 (None이면 처음부터)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    FName StartSection = NAME_None;

    //종료 요청까지의 시간 (초). 0이면 종료 요청 없이 몽타주가 끝나거나 상태를 벗어날 때까지 재생
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
    float Duration = 0.0f;

    //Duration 랜덤 편차 (초)
    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
    float RandomDeviation = 0.0f;

    //Duration 경과 시 현재 섹션이 끝난 뒤 이어갈 섹션. None이면 블렌드 아웃으로 정지
    //Next Section이 없는 종료 섹션이어야 함 (아니면 EnterState 실패)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    FName EndSection = NAME_None;

    //Context에 바인딩된 AI 컨트롤러 (워프 타깃 조회용)
    UPROPERTY(EditAnywhere, Category = "Context")
    TObjectPtr<AEnemyAIController> EnemyAIController = nullptr;

    //몽타주 MotionWarping ANS의 Warp Target Name과 일치해야 함. None이면 워프 타깃을 세팅하지 않음
    //타깃은 EnemyAttackAbility와 같이 AIController의 CurrentTarget 사용 (없으면 워프 타깃 제거 → 회전 없음)
    //어빌리티가 쓰는 AttackTarget과 겹치지 않는 이름 사용 (연출 몽타주는 Facing)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    FName WarpTargetName = NAME_None;

    //이번 실행의 실제 Duration
    float ComputedDuration = 0.0f;

    //EnterState 이후 경과 시간
    float ElapsedTime = 0.0f;

    //종료 요청 여부
    bool bEndRequested = false;

    //이 태스크가 워프 타깃을 세팅했는지 여부 (ExitState 정리 대상 판단)
    bool bWarpTargetSet = false;
};

/**
 * ASC를 통해 몽타주를 재생하는 Task (어빌리티 없음)
 * 게임플레이 데이터가 없는 연출 몽타주(대기, 경계, 하울링 등) 전용. 히트 판정·쿨다운이 있는 몽타주는 어빌리티로 재생
 * ASC::PlayMontage의 RepAnimMontageInfo로 시뮬레이티드 프록시에 복제
 * 몽타주가 끝나거나 다른 몽타주로 대체되면 Succeeded
 * 워프 방식(구간·회전 속도)은 몽타주 MotionWarping ANS에서 정의, 이 태스크는 워프 타깃 등록만 담당 (EnemyAttackAbility와 동일한 분담)
 * WarpTargetName 지정 시 재생 전 워프 타깃을 액터 추적으로 세팅하고 ExitState에서 제거 (워프는 루트 모션이 켜진 시퀀스에서만 적용)
 */
USTRUCT(meta = (DisplayName = "Play Montage (ASC)", Category = "GAS"))
struct ACTIONPRACTICE_API FPlayMontageTask : public FStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FPlayMontageTaskInstanceData;

    FPlayMontageTask() = default;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
