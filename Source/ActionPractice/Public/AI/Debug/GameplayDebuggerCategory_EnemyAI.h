#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU
#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;
class AEnemyCharacter;

/**
 * 게임플레이 디버거(') "EnemyAI" 카테고리
 * - 디버그 대상 적의 시야(Sight/LoseSight 부채꼴)와 StateTree 거리 파라미터(Melee/Middle/Long/Alert) 링을 바닥에 표시
 * - 활성 StateTree 상태, 현재 타깃과 거리를 텍스트로 표시
 * - AI는 서버에서만 동작하므로 서버 CollectData에서 수집한 텍스트/셰이프를 디버거가 클라이언트로 복제 (데디 서버에서도 확인 가능)
 */
class FGameplayDebuggerCategory_EnemyAI : public FGameplayDebuggerCategory
{
public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	FGameplayDebuggerCategory_EnemyAI();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	//시야 부채꼴 (호 + 양 끝 반경선)
	void AddSightArc(const FVector& Center, const FVector& Forward, float Radius, float HalfAngleDegrees, const FColor& Color, const FString& Description);

	//StateTree 파라미터 거리 링과 텍스트
	void CollectStateTreeRanges(const AEnemyCharacter* Enemy, const FVector& Center);

	//시야 설정과 부채꼴
	void CollectPerception(const AEnemyCharacter* Enemy, const FVector& Center);

	//활성 상태, 타깃, 거리
	void CollectStateAndTarget(const AEnemyCharacter* Enemy);

#pragma endregion
};

#endif // WITH_GAMEPLAY_DEBUGGER_MENU
