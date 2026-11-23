#include "UI/TitleScreenWidget.h"
#include "UI/ControlsWidget.h"
#include "UI/MenuButtonWidget.h"
#include "Components/Overlay.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogTitleScreenWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogTitleScreenWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UTitleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ControlsButton)
	{
		ControlsButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UTitleScreenWidget::OnControlsButtonClicked);
	}

	if (TestMapButton)
	{
		TestMapButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UTitleScreenWidget::OnTestMapButtonClicked);
	}

	if (MainMapButton)
	{
		MainMapButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UTitleScreenWidget::OnMainMapButtonClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnMenuButtonClicked.AddUniqueDynamic(this, &UTitleScreenWidget::OnQuitButtonClicked);
	}

	//조작법 위젯 생성 (초기 Collapsed)
	if (!ControlsWidget)
	{
		ControlsWidget = UControlsWidget::CreateInOverlay(this, ControlsWidgetClass, ControlsOverlay);

		if (ControlsWidget)
		{
			ControlsWidget->OnControlsClosed.AddUObject(this, &UTitleScreenWidget::HandleControlsClosed);
		}
	}

	//등장 페이드인 — 디자이너 미리보기에서는 투명 처리하지 않음
	if (!IsDesignTime() && (LogoBox || MenuBox))
	{
		FadeInElapsed = 0.f;
		bFadeInFinished = false;

		if (LogoBox)
		{
			LogoBox->SetRenderOpacity(0.f);
		}

		//투명한 동안 보이지 않는 버튼이 눌리지 않도록 페이드인이 끝날 때까지 클릭 차단
		if (MenuBox)
		{
			MenuBox->SetRenderOpacity(0.f);
			MenuBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UTitleScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bFadeInFinished) return;

	FadeInElapsed += InDeltaTime;

	const float LogoAlpha = GetFadeAlpha(FadeInElapsed, 0.f, LogoFadeInDuration);
	const float MenuAlpha = GetFadeAlpha(FadeInElapsed, MenuFadeInDelay, MenuFadeInDuration);

	if (LogoBox)
	{
		LogoBox->SetRenderOpacity(LogoAlpha);
	}

	if (MenuBox)
	{
		MenuBox->SetRenderOpacity(MenuAlpha);
	}

	bFadeInFinished = LogoAlpha >= 1.f && MenuAlpha >= 1.f;

	//페이드인 중에는 조작법을 열 수 없으므로 HitTestInvisible 상태만 복원
	if (bFadeInFinished && MenuBox && MenuBox->GetVisibility() == ESlateVisibility::HitTestInvisible)
	{
		MenuBox->SetVisibility(ESlateVisibility::Visible);
	}
}

float UTitleScreenWidget::GetFadeAlpha(float Elapsed, float Delay, float Duration)
{
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return Elapsed >= Delay ? 1.f : 0.f;
	}

	//EaseOut으로 끝부분을 부드럽게
	const float Linear = FMath::Clamp((Elapsed - Delay) / Duration, 0.f, 1.f);
	return 1.f - FMath::Square(1.f - Linear);
}

void UTitleScreenWidget::OnControlsButtonClicked()
{
	if (!ControlsWidget) return;

	//조작법이 열린 동안 메뉴 버튼은 숨겨 클릭·겹침 방지
	if (MenuBox)
	{
		MenuBox->SetVisibility(ESlateVisibility::Hidden);
	}

	ControlsWidget->OpenControls();
	DEBUG_LOG(TEXT("Controls opened"));
}

void UTitleScreenWidget::HandleControlsClosed()
{
	if (MenuBox)
	{
		MenuBox->SetVisibility(ESlateVisibility::Visible);
	}

	if (ControlsButton)
	{
		ControlsButton->FocusButton();
	}

	DEBUG_LOG(TEXT("Controls closed"));
}

void UTitleScreenWidget::OnTestMapButtonClicked()
{
	OpenLevelSafe(TestMapLevel, TEXT("TestMap"));
}

void UTitleScreenWidget::OnMainMapButtonClicked()
{
	OpenLevelSafe(MainMapLevel, TEXT("MainMap"));
}

void UTitleScreenWidget::OnQuitButtonClicked()
{
	DEBUG_LOG(TEXT("Quit button clicked — exiting game"));
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UTitleScreenWidget::OpenLevelSafe(const TSoftObjectPtr<UWorld>& Level, const TCHAR* DebugName)
{
	if (Level.IsNull())
	{
		DEBUG_LOG(TEXT("OpenLevel: %s level not set"), DebugName);
		return;
	}

	const FString MapPath = Level.GetLongPackageName();
	DEBUG_LOG(TEXT("Opening %s: %s"), DebugName, *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}
