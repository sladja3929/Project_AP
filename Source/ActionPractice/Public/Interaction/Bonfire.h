#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IInteractable.h"
#include "Bonfire.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USceneComponent;

UCLASS()
class ACTIONPRACTICE_API ABonfire : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	ABonfire();

	//IInteractable 구현
	virtual bool CanInteract(AActor* InInstigator) const override;
	virtual void Interact(AActor* InInstigator) override;
	virtual FText GetInteractionPrompt() const override;

	FORCEINLINE bool IsDefaultSpawnPoint() const { return bIsDefaultSpawnPoint; }
	FORCEINLINE FVector GetRespawnLocation() const { return RespawnPoint->GetComponentLocation(); }
	FORCEINLINE FRotator GetRespawnRotation() const { return RespawnPoint->GetComponentRotation(); }
	FORCEINLINE FTransform GetRespawnTransform() const { return RespawnPoint->GetComponentTransform(); }

#pragma endregion

protected:
#pragma region "Protected Variables"

	//게임 시작 시 기본 리스폰 포인트로 사용할 Bonfire 지정 (맵당 1개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonfire")
	bool bIsDefaultSpawnPoint = false;

	//상호작용 프롬프트 텍스트 — BP에서 수정 가능, 다국어 대응
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bonfire")
	FText InteractionPromptText = NSLOCTEXT("Bonfire", "DefaultPrompt", "휴식");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractionSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RespawnPoint = nullptr;

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
