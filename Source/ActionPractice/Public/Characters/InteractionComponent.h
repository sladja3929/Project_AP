#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableChanged, AActor*, NewInteractable);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractableChanged OnInteractableChanged;

#pragma endregion

#pragma region "Public Functions"

	UInteractionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void TryInteract();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetCurrentInteractable() const;

	UFUNCTION(Server, Reliable)
	void Server_TryInteract(AActor* InInteractable);

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractionRadius = 200.f;

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AActor> CurrentInteractable = nullptr;

	FTimerHandle InteractionTimerHandle;

#pragma endregion

#pragma region "Private Functions"

	//주기적으로 액터 탐지
	void SearchForInteractable();

#pragma endregion
};
