#include "AI/StateTree/GASStateTreeAIComponent.h"

TSubclassOf<UStateTreeSchema> UGASStateTreeAIComponent::GetSchema() const
{
	return UGASStateTreeAIComponentSchema::StaticClass();
}

bool UGASStateTreeAIComponent::GetStateTreeParameterFloat(FName ParameterName, float& OutValue) const
{
	//StateTreeRef는 부모 클래스의 protected 멤버라 서브클래스에서 접근 경로 제공
	const TValueOrError<float, EPropertyBagResult> Result = StateTreeRef.GetParameters().GetValueFloat(ParameterName);

	if (Result.HasError())
	{
		return false;
	}

	OutValue = Result.GetValue();
	return true;
}
