#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "StateTreeTaskBase.h"
#include "GameplayTagContainer.h"
#include "ActivateAbilityTask.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Task Instance Data
 * Context: 스키마에서 설정한 Context에서 자동으로 같은 자료형을 찾아 바인딩
 * Input/Parameter: 에디터에서 설정하는 입력 값 (직접 입력 or 다른 노드의 Output 연결)
 * Output: 노드의 결과 값
 */

USTRUCT()
struct FActivateAbilityTaskInstanceData
{
    GENERATED_BODY()
    
    //Context에 바인딩된 ASC
    UPROPERTY(EditAnywhere, Category = "Context")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

    //실행할 어빌리티 클래스를 에셋에서 지정
    UPROPERTY(EditAnywhere, Category = "Parameter")
    TSubclassOf<UGameplayAbility> AbilityToActivate = nullptr;

    //어빌리티 종료 후 대기 시간 (초)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    float EndDelay = 0.0f;

    //spec 핸들
    FGameplayAbilitySpecHandle AbilityHandle;

    //어빌리티 종료 델리게이트 핸들
    FDelegateHandle AbilityEndedHandle;

    //어빌리티 종료 여부
    bool bAbilityEnded = false;

    //어빌리티가 취소되었는지 여부
    bool bAbilityCancelled = false;

    //EndDelay 경과 시간
    float ElapsedEndDelay = 0.0f;
};

/**
 * GAS Ability를 실행하는 Task
 */
USTRUCT(meta = (DisplayName = "Activate GAS Ability", Category = "GAS"))
struct ACTIONPRACTICE_API FActivateAbilityTask : public FStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FActivateAbilityTaskInstanceData;

    FActivateAbilityTask() = default;
    
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
