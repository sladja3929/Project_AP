#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Characters/EnemyCharacter.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAttributeSet, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAttributeSet, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UEnemyAttributeSet::UEnemyAttributeSet()
{
	InitStance(200.0f);
	InitMaxStance(200.0f);
	InitStanceRegenRate(5.0f);
	InitPhysicalAttackPower(50.0f);
}

void UEnemyAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UEnemyAttributeSet, Stance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEnemyAttributeSet, MaxStance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEnemyAttributeSet, StanceRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEnemyAttributeSet, PhysicalAttackPower, COND_None, REPNOTIFY_Always);
}

void UEnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxStanceAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetStanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStance());
	}
	else if (Attribute == GetStanceRegenRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UEnemyAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxStanceAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetStanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStance());
	}
	else if (Attribute == GetStanceRegenRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	//Health가 변경되었을 때 머리 위 HP바 표시
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ASC->GetAvatarActor()))
			{
				Enemy->ShowEnemyHealthBar();
			}
		}
	}
}

// Rep Notify Functions
void UEnemyAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	Super::OnRep_Health(OldHealth);

	//클라이언트: Health 복제 수신 시 HP바 표시 (서버는 PostGameplayEffectExecute에서 처리)
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ASC->GetAvatarActor()))
		{
			Enemy->ShowEnemyHealthBar();
		}
	}
}

void UEnemyAttributeSet::OnRep_Stance(const FGameplayAttributeData& OldStance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnemyAttributeSet, Stance, OldStance);
}

void UEnemyAttributeSet::OnRep_MaxStance(const FGameplayAttributeData& OldMaxStance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnemyAttributeSet, MaxStance, OldMaxStance);
}

void UEnemyAttributeSet::OnRep_StanceRegenRate(const FGameplayAttributeData& OldStanceRegenRate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnemyAttributeSet, StanceRegenRate, OldStanceRegenRate);
}

void UEnemyAttributeSet::OnRep_PhysicalAttackPower(const FGameplayAttributeData& OldPhysicalAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnemyAttributeSet, PhysicalAttackPower, OldPhysicalAttackPower);
}
