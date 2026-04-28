#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInteractable.generated.h"

UINTERFACE(MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class ACTIONPRACTICE_API IInteractable
{
	GENERATED_BODY()

public:

	//상호작용 가능 여부 판별
	virtual bool CanInteract(AActor* InInstigator) const = 0;

	//대상 상호작용 로직
	virtual void Interact(AActor* InInstigator) = 0;

	//상호작용 안내 텍스트 반환
	virtual FText GetInteractionPrompt() const = 0;
};
