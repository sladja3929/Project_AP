#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "DebugLogTask.generated.h"

class AActor;

USTRUCT()
struct ACTIONPRACTICE_API FDebugLogTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	//로그 앞에 붙는 구분 문자열
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FString Message;

	//바인딩해서 상태 진입 시점 값을 기록할 입력값
	UPROPERTY(EditAnywhere, Category = "Input")
	float Value1 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Value2 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Value3 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Value4 = 0.0f;
};

/**
 * 상태 진입 시 메시지와 바인딩된 값 4개를 로그로 남기는 측정용 태스크
 * -원인 조사용 임시 로그에 쓰고, 조사가 끝나면 노드를 제거할 것
 * -완료하지 않고 Running을 유지하므로 상태 완료는 다른 태스크가 결정
 */
USTRUCT(meta = (DisplayName = "Debug Log"))
struct ACTIONPRACTICE_API FDebugLogTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDebugLogTaskInstanceData;

	FDebugLogTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
