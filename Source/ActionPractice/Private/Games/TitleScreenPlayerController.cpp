#include "Games/TitleScreenPlayerController.h"
#include "UI/TitleScreenWidget.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTitleScreenPlayerController, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogTitleScreenPlayerController, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void ATitleScreenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	//데디케이티드 서버에서는 UI 생성 안 함
	if (!IsLocalController()) return;

	if (!TitleScreenWidgetClass) return;

	TitleScreenWidget = CreateWidget<UTitleScreenWidget>(this, TitleScreenWidgetClass);
	if (TitleScreenWidget)
	{
		TitleScreenWidget->AddToViewport();
		DEBUG_LOG(TEXT("TitleScreenWidget created and added to viewport"));
	}

	//마우스 커서 표시 + UI 전용 입력 모드
	bShowMouseCursor = true;
	SetInputMode(FInputModeUIOnly());
}
