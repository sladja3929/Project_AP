#include "UI/EnemyHealthBarWidget.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "Components/ProgressBar.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyHealthBarWidget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyHealthBarWidget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HealthBar)
	{
		HealthBar->SetPercent(1.0f);
	}

	if (HealthDamageBar)
	{
		HealthDamageBar->SetPercent(1.0f);
	}
}

void UEnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CachedAttributeSet || !HealthBar) return;

	const float MaxHealth = CachedAttributeSet->GetMaxHealth();
	if (MaxHealth <= 0.0f) return;

	const float NewPercent = FMath::Clamp(CachedAttributeSet->GetHealth() / MaxHealth, 0.0f, 1.0f);

	//HP 감소
	if (NewPercent < CurrentHealthPercent)
	{
		HealthBar->SetPercent(NewPercent);
		TargetHealthDamagePercent = NewPercent;
		HealthDamageDelayTimer = 0.0f;
	}
	//HP 증가
	else if (NewPercent > CurrentHealthPercent)
	{
		HealthBar->SetPercent(NewPercent);

		if (HealthDamageBar)
		{
			HealthDamageBar->SetPercent(NewPercent);
		}

		TargetHealthDamagePercent = NewPercent;
	}

	CurrentHealthPercent = NewPercent;

	UpdateDamageBars(InDeltaTime);
}

void UEnemyHealthBarWidget::UpdateDamageBars(float DeltaTime)
{
	if (HealthDamageBar && HealthDamageBar->GetPercent() > TargetHealthDamagePercent)
	{
		HealthDamageDelayTimer += DeltaTime;

		if (HealthDamageDelayTimer >= DamageBarDelayTime)
		{
			const float CurrentPercent = HealthDamageBar->GetPercent();
			const float NewPercent = FMath::FInterpTo(CurrentPercent, TargetHealthDamagePercent, DeltaTime, DamageBarLerpSpeed);
			HealthDamageBar->SetPercent(NewPercent);
		}
	}
}

void UEnemyHealthBarWidget::SetAttributeSet(UEnemyAttributeSet* InAttributeSet)
{
	CachedAttributeSet = InAttributeSet;
	DEBUG_LOG(TEXT("EnemyHealthBarWidget: AttributeSet set"));
}
