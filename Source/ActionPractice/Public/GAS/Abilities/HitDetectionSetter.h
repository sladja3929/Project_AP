#pragma once

#include "CoreMinimal.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "Items/AttackData.h"
#include "GameplayTagContainer.h"
#include "HitDetectionSetter.generated.h"
 
/**
 * HitDetection 기능을 사용하는 클래스가 구현해야 하는 인터페이스
 */
UINTERFACE(MinimalAPI)
class UHitDetectionUser : public UInterface
{
	GENERATED_BODY()
};

class ACTIONPRACTICE_API IHitDetectionUser
{
	GENERATED_BODY()

public:

	//HitDetection 설정을 담당
	virtual void SetHitDetectionConfig() = 0;

	//Hit 감지 콜백
	virtual void OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData) = 0;

};

/**
 * HitDetection 기능을 캡슐화한 구조체
 * IHitDetectionUser를 구현한 어빌리티에서 사용
 */
USTRUCT()
struct ACTIONPRACTICE_API FHitDetectionSetter
{
	GENERATED_BODY()

public:

	//HitDetection 인터페이스 가져오기
	bool Init(const TScriptInterface<IHitDetectionInterface>& InHitDetection);
	
	bool Bind(IHitDetectionUser* User);
	void UnBind();

	//PrepareHitDetection 호출
	bool PrepareHitDetection(const FGameplayTagContainer& AssetTag, const int32 ComboCounter);
	bool PrepareHitDetection(const FName& AttackName, const int32 ComboCounter);

	bool IsValid() const;

private:

	UPROPERTY()
	TScriptInterface<IHitDetectionInterface> HitDetection;

	FDelegateHandle OnHitDelegateHandle;
};