#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_StaminaRegen.generated.h"

//Periodic GE에서 매 실행마다 현재 StaminaRegenRate를 라이브로 읽어 Stamina에 적용한다.
//Modifier의 Spec 캐싱 문제를 우회하기 위한 Execution Calculation 클래스.
UCLASS()
class ACTIONPRACTICE_API UExecCalc_StaminaRegen : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UExecCalc_StaminaRegen();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
