#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Lunging.generated.h"

//Lunge 어빌리티에서 코드 이동 구간을 정의하는 ANS
//Begin에서 ANS 지속시간(TotalDuration)을 EventMagnitude로 전달 → 어빌리티가 이동 시작
//End에서 이동 종료 이벤트 전달 → 어빌리티가 이동 태스크 정리
UCLASS(meta = (DisplayName = "Lunging"))
class ACTIONPRACTICE_API UAnimNotifyState_Lunging : public UAnimNotifyState
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Lunging");
	}

#pragma endregion
};
