#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TitleScreenGameMode.generated.h"

//타이틀 화면 전용 GameMode
//캐릭터 스폰 없이 UI만 표시
UCLASS()
class ACTIONPRACTICE_API ATitleScreenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	ATitleScreenGameMode();

#pragma endregion
};
