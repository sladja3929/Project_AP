#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_TrackingTarget.generated.h"

//Lunge 어빌리티에서 타겟 추적 구간을 정의하는 ANS
//Tick마다 적을 타겟 방향으로 회전시키고, ASC 이벤트로 타겟 위치를 전달
//Lunging ANS와 겹칠 경우 이동 중에도 목적지가 실시간 갱신됨
UCLASS(meta = (DisplayName = "Tracking Target"))
class ACTIONPRACTICE_API UAnimNotifyState_TrackingTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//프레임당 회전 보간 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	float RotateInterpTime = 0.05f;

#pragma endregion

#pragma region "Public Functions"

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Tracking Target");
	}

#pragma endregion
};
