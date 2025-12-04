#include "UI/BossHealthWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/AttributeSet/BossAttributeSet.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossHealthWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBossHealthWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UBossHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BossHealthBar)
	{
		BossHealthBar->SetPercent(1.0f);
	}

	if (BossHealthDamageBar)
	{
		BossHealthDamageBar->SetPercent(1.0f);
	}

	DEBUG_LOG(TEXT("BossHealthWidget Constructed"));
}

void UBossHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (BossAttributeSet)
	{
		UpdateBossHealth(BossAttributeSet->GetHealth(), BossAttributeSet->GetMaxHealth());
	}

	UpdateDamageBars(InDeltaTime);
}

void UBossHealthWidget::SetBossAttributeSet(UBossAttributeSet* InAttributeSet)
{
	if (!InAttributeSet)
	{
		DEBUG_LOG(TEXT("BossAttributeSet Invalid!"));
		return;
	}

	BossAttributeSet = InAttributeSet;
	DEBUG_LOG(TEXT("BossAttributeSet set to BossHealthWidget"));
}

void UBossHealthWidget::SetBossName(const FName& InBossName)
{
	if (!BossNameText)
	{
		DEBUG_LOG(TEXT("BossNameText is nullptr"));
		return;
	}

	FText BossNameAsText = FText::FromName(InBossName);
	BossNameText->SetText(BossNameAsText);

	DEBUG_LOG(TEXT("Boss name set to: %s"), *InBossName.ToString());
}

void UBossHealthWidget::UpdateBossHealth(float CurrentHealth, float MaxHealth)
{
	if (!BossHealthBar || MaxHealth <= 0.0f)
	{
		return;
	}

	float NewHealthPercent = CurrentHealth / MaxHealth;

	//Boss HP 감소
	if (NewHealthPercent < CurrentBossHealthPercent)
	{
		BossHealthBar->SetPercent(NewHealthPercent);
		TargetBossHealthDamagePercent = NewHealthPercent;
		CurrentBossHealthDelayTimer = 0.0f;
	}

	//Boss HP 증가
	else if (NewHealthPercent > CurrentBossHealthPercent)
	{
		BossHealthBar->SetPercent(NewHealthPercent);

		if (BossHealthDamageBar)
		{
			BossHealthDamageBar->SetPercent(NewHealthPercent);
		}

		TargetBossHealthDamagePercent = NewHealthPercent;
	}

	CurrentBossHealthPercent = NewHealthPercent;
}

void UBossHealthWidget::UpdateDamageBars(float DeltaTime)
{
	if (BossHealthDamageBar && BossHealthDamageBar->GetPercent() > TargetBossHealthDamagePercent)
	{
		CurrentBossHealthDelayTimer += DeltaTime;

		if (CurrentBossHealthDelayTimer >= DamageBarDelayTime)
		{
			float CurrentPercent = BossHealthDamageBar->GetPercent();
			float NewPercent = FMath::FInterpTo(CurrentPercent, TargetBossHealthDamagePercent, DeltaTime, DamageBarLerpSpeed);
			BossHealthDamageBar->SetPercent(NewPercent);
			DEBUG_LOG(TEXT("Boss HP Lerp Applied: NewPercent=%f"), NewPercent);
		}
	}
}

void UBossHealthWidget::NativeDestruct()
{
	Super::NativeDestruct();

	DEBUG_LOG(TEXT("BossHealthWidget Destructed"));
}