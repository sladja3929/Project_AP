#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IInteractable.h"
#include "PickupItem.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USceneComponent;
class UBaseItemDataAsset;

UCLASS()
class ACTIONPRACTICE_API APickupItem : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
#pragma endregion

#pragma region "Public Functions"

	APickupItem();

	//IInteractable 구현
	virtual bool CanInteract(AActor* InInstigator) const override;
	virtual void Interact(AActor* InInstigator) override;
	virtual FText GetInteractionPrompt() const override;

	//획득 완료 후 호출 — 비활성화 및 제거
	void OnPickedUp();

	FORCEINLINE UBaseItemDataAsset* GetItemDA() const { return ItemDA; }
	FORCEINLINE int32 GetItemCount() const { return ItemCount; }

#pragma endregion

protected:
#pragma region "Protected Variables"

	//줍는 아이템 정의 (DA 레퍼런스) — 확장 가능하도록 Base 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	TObjectPtr<UBaseItemDataAsset> ItemDA = nullptr;

	//줍는 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	int32 ItemCount = 1;

	//상호작용 프롬프트 텍스트 — 비어있으면 DA의 DisplayName 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	FText InteractionPromptText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere = nullptr;

#pragma endregion

#pragma region "Protected Functions"
#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
