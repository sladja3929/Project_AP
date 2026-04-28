#include "Games/TitleScreenGameMode.h"
#include "Games/TitleScreenPlayerController.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTitleScreenGameMode, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogTitleScreenGameMode, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ATitleScreenGameMode::ATitleScreenGameMode()
{
	//타이틀 화면에서는 Pawn을 스폰하지 않음
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ATitleScreenPlayerController::StaticClass();
}
