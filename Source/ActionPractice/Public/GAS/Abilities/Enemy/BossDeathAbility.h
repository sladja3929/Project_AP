#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Enemy/EnemyDeathAbility.h"
#include "BossDeathAbility.generated.h"

UCLASS()
class ACTIONPRACTICE_API UBossDeathAbility : public UEnemyDeathAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UBossDeathAbility();

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

	//보스 전용 서버 사망 처리 (BGM 정지, 보스 HP바 숨김)
	virtual void ExecuteDeathServerLogic() override;

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

	void StopBossBGMAndHideUI();

#pragma endregion
};
