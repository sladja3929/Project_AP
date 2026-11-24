#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_HitDetectionBurst.generated.h"

//착지·충격 순간 원형 판정 이벤트 (Event.Notify.HitDetectionBurst)를 ASC로 전달
//반경과 위치는 공격 데이터의 소켓 설정(FAttackSocketConfig::BurstRadius)에서 결정되며, 이 노티파이는 시점만 정함
UCLASS(meta = (DisplayName = "Hit Detection Burst"))
class ACTIONPRACTICE_API UAnimNotify_HitDetectionBurst : public UAnimNotify
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UAnimNotify_HitDetectionBurst();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#pragma endregion
};
