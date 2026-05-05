#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_TrackingTarget.generated.h"

UCLASS(meta = (DisplayName = "Tracking Target"))
class ACTIONPRACTICE_API UAnimNotifyState_TrackingTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//프레임당 회전 보간 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	float RotateInterpTime = 0.05f;

	//destination 이벤트 발사 주기 (초). 0 이하면 timer 비활성화 (Begin/End만 발사)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0.0"))
	float DispatchInterval = 0.2f;

	//destination 이벤트 발사 최소 변화량 (cm). 마지막 발사 위치와의 거리가 이 값 미만이면 skip
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0.0"))
	float MinDispatchDistance = 5.0f;

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

private:
#pragma region "Private Variables"

	//ANS 인스턴스 단위 상태 — ANS는 같은 montage를 쓰는 여러 적이 공유하므로 적별로 분리 필요
	struct FTrackingState
	{
		float AccumulatedTime = 0.0f;
		FVector LastDispatchedDestination = FVector::ZeroVector;
	};

	//key: NotifyBegin/Tick/End의 호출 컴포넌트. NotifyEnd에서 제거
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FTrackingState> ActiveStates;

#pragma endregion
};
