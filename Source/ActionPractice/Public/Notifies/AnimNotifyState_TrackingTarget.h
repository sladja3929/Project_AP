#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_TrackingTarget.generated.h"

//Lunge 어빌리티에서 타겟 추적 구간을 정의하는 ANS
//Tick마다 적을 타겟 방향으로 회전시키고, Begin/End 시점에만 ASC 이벤트로 타겟 위치를 전달
//Lunging ANS와 겹친 배치라면 End 시점에 진행 중인 RootMotion target이 1회 갱신됨 (구간 시작/종료 두 시점만 갱신)
//매 틱 destination 갱신 패턴은 시뮬레이트 프록시 측 RootMotionSource ID 변환 race를 유발하므로 회피
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
