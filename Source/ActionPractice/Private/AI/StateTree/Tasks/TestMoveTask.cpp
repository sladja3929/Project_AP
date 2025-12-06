// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/StateTree/Tasks/TestMoveTask.h"
#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTestMoveTask, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogTestMoveTask, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

EStateTreeRunStatus FTestMoveTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Actor)
	{
		DEBUG_LOG(TEXT("TestMoveTask: Actor is not valid"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime = 0.0f;

	DEBUG_LOG(TEXT("TestMoveTask: Started moving forward for %.1f seconds at speed %.1f"), InstanceData.Duration, InstanceData.Speed);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTestMoveTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Actor)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;

	// Duration 경과 체크
	if (InstanceData.ElapsedTime >= InstanceData.Duration)
	{
		DEBUG_LOG(TEXT("TestMoveTask: Completed after %.2f seconds"), InstanceData.ElapsedTime);
		return EStateTreeRunStatus::Succeeded;
	}

	// 액터를 앞으로 이동
	FVector CurrentLocation = InstanceData.Actor->GetActorLocation();
	FVector ForwardVector = InstanceData.Actor->GetActorForwardVector();
	FVector NewLocation = CurrentLocation + (ForwardVector * InstanceData.Speed * DeltaTime);

	InstanceData.Actor->SetActorLocation(NewLocation);

	return EStateTreeRunStatus::Running;
}

void FTestMoveTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	DEBUG_LOG(TEXT("TestMoveTask: Exited after %.2f seconds"), InstanceData.ElapsedTime);
}
