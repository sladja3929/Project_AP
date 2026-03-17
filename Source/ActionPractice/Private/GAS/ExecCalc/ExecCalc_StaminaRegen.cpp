#include "GAS/ExecCalc/ExecCalc_StaminaRegen.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogExecCalc_StaminaRegen, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogExecCalc_StaminaRegen, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

//캡처 대상 어트리뷰트를 로컬 구조체로 선언 (ExecCalc 표준 패턴)
struct FStaminaRegenStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(StaminaRegenRate);

	FStaminaRegenStatics()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UBaseAttributeSet, StaminaRegenRate, Source, false);
	}
};

static const FStaminaRegenStatics& StaminaRegenStatics()
{
	static FStaminaRegenStatics Statics;
	return Statics;
}

UExecCalc_StaminaRegen::UExecCalc_StaminaRegen()
{
	RelevantAttributesToCapture.Add(StaminaRegenStatics().StaminaRegenRateDef);
}

void UExecCalc_StaminaRegen::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	if (!SourceASC)
	{
		return;
	}

	//캡처 메커니즘 대신 ASC에서 현재 값을 직접 읽기
	const float StaminaRegenRate = SourceASC->GetNumericAttribute(UBaseAttributeSet::GetStaminaRegenRateAttribute());

	DEBUG_LOG(TEXT("Execute - StaminaRegenRate (Direct Read): %f"), StaminaRegenRate);

	if (StaminaRegenRate != 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UBaseAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, StaminaRegenRate));
	}
}
