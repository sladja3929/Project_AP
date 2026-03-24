#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "DefensePolicy.generated.h"

struct FFinalAttackData;
struct FGameplayEventData;

//방어 판정 결과 (Impact Cue 분기용)
UENUM(BlueprintType)
enum class EDefenseResult : uint8
{
	None,         //일반 피격
	Blocked,      //가드 성공
	GuardBroken,  //가드 브레이크
	//Parried,    //패리 성공 (추후 추가)
};

UINTERFACE(MinimalAPI, Blueprintable)
class UDefensePolicy : public UInterface
{
	GENERATED_BODY()
};

class ACTIONPRACTICE_API IDefensePolicy
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	//OnDamagedPreResolve 델리게이트 수신
	virtual void OnDamaged(AActor* SourceActor, const FFinalAttackData& FinalAttackData) = 0;

	//수식 계산 및 최종 attribute 설정
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) = 0;

	//최종 계산 후 피격 로직 트리거
	virtual void HandleOnDamagedResolved(AActor* SourceActor, const FFinalAttackData& FinalAttackData) = 0;

	//HitReaction Ability EventData 준비
	virtual void PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData) = 0;

#pragma endregion
};