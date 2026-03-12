#include "UI/DeathScreenWidget.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogDeathScreenWidget, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogDeathScreenWidget, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

void UDeathScreenWidget::HandleDeadStateStart()
{
	SetDeathScreenVisibility(true);
	StartFadeIn();
}

void UDeathScreenWidget::HandleDeadStateFinish()
{
	StartFadeOut();
	//FadeOut의 Finished 델리게이트에 SetDeathScreenVisibility(false) 바인딩할 것
}

void UDeathScreenWidget::SetDeathScreenVisibility(bool bShow)
{
	ESlateVisibility InVisibility = bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	
	SetVisibility(InVisibility);
	SetIsEnabled(bShow);
	
	DEBUG_LOG(TEXT("DeathScreen Visibility: %s"), bShow ? TEXT("Enabled") : TEXT("Disabled"));
}

void UDeathScreenWidget::StartFadeOut_Implementation()
{
	//FadeOut Finished에 델리게이트 바인딩이 되지 않았을 경우 즉시 안보이게 설정
	DEBUG_LOG(TEXT("StartFadeOut: Base Implementation Executed"));
	SetDeathScreenVisibility(false);
}
