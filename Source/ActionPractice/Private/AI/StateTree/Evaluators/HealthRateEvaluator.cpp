// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/StateTree/Evaluators/HealthRateEvaluator.h"
#include "AbilitySystemComponent.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"
#include "StateTreeExecutionContext.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogHealthRateEvaluator, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogHealthRateEvaluator, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

void FHealthRateEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAbilitySystemComponent)
	{
		DEBUG_LOG(TEXT("AbilitySystemComponent is not valid in HealthRateEvaluator"));
	}
	else
	{
		DEBUG_LOG(TEXT("AbilitySystemComponent Bind Successfully"));
	}

	DEBUG_LOG(TEXT("HealthRateEvaluator TreeStart"));
}

void FHealthRateEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.HealthRate = 1.0f;

	DEBUG_LOG(TEXT("HealthRateEvaluator TreeStop"));
}

void FHealthRateEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UpdateHealthRate(Context);
}

void FHealthRateEvaluator::UpdateHealthRate(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.EnemyAbilitySystemComponent)
	{
		InstanceData.HealthRate = 1.0f;
		return;
	}

	const UBaseAttributeSet* AttributeSet = InstanceData.EnemyAbilitySystemComponent->GetSet<UBaseAttributeSet>();
	if (!AttributeSet)
	{
		DEBUG_LOG(TEXT("BaseAttributeSet is not valid in HealthRateEvaluator"));
		InstanceData.HealthRate = 1.0f;
		return;
	}
	
	InstanceData.HealthRate = AttributeSet->GetHealthPercent();
}