#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ProjectileEvent.generated.h"

UENUM(BlueprintType)
enum class EProjectileNotifyEvent : uint8
{
	//투사체 생성. DA의 bPresentBeforeGrab이면 오너 앞 등장 위치에 놓고, 아니면 바로 손 소켓에 부착
	Spawn UMETA(DisplayName = "Spawn"),
	//등장해 있던 투사체를 손 소켓에 부착 (잡는 프레임)
	Grab UMETA(DisplayName = "Grab"),
	//들고 있던 투사체를 타깃 방향으로 발사 (손을 놓는 프레임)
	Launch UMETA(DisplayName = "Launch"),
};

//투사체 어빌리티에 생성·발사 시점을 GameplayEvent로 전달
UCLASS(meta = (DisplayName = "Projectile Event"))
class ACTIONPRACTICE_API UAnimNotify_ProjectileEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	EProjectileNotifyEvent EventType = EProjectileNotifyEvent::Spawn;

#pragma endregion

#pragma region "Public Functions"

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#pragma endregion
};
