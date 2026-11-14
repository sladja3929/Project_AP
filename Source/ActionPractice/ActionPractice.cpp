#include "ActionPractice.h"
#include "Modules/ModuleManager.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU
#include "GameplayDebugger.h"
#include "AI/Debug/GameplayDebuggerCategory_EnemyAI.h"
#endif

namespace ActionPracticeModule
{
#if WITH_GAMEPLAY_DEBUGGER_MENU
	static const FName EnemyAIDebuggerCategoryName(TEXT("EnemyAI"));
#endif
}

//게임플레이 디버거 커스텀 카테고리 등록을 위한 게임 모듈
class FActionPracticeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER_MENU
		IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
		GameplayDebugger.RegisterCategory(ActionPracticeModule::EnemyAIDebuggerCategoryName,
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_EnemyAI::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
		GameplayDebugger.NotifyCategoriesChanged();
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_GAMEPLAY_DEBUGGER_MENU
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
			GameplayDebugger.UnregisterCategory(ActionPracticeModule::EnemyAIDebuggerCategoryName);
			GameplayDebugger.NotifyCategoriesChanged();
		}
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FActionPracticeModule, ActionPractice, "ActionPractice" );
