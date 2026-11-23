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
	//데미지·회복은 메타 어트리뷰트(IncomingDamage/IncomingHealing)로 들어와 Super 안의 피격 파이프라인이 SetHealth로 직접 바꿈
	//GE가 바꾼 어트리뷰트만 보면 Health 변화를 놓치므로 Super 전후 값으로 판단
	const float OldHealth = GetHealth();

	Super::PostGameplayEffectExecute(Data);

	if (!FMath::IsNearlyEqual(OldHealth, GetHealth()))
	{
		ShowHealthBarOnHealthChanged();
	}
}

void UEnemyAttributeSet::ShowHealthBarOnHealthChanged() const
{
	//사망 시 숨김은 사망 어빌리티 담당이라 다시 띄우지 않음
	if (GetHealth() <= 0.0f)
	{
		return;
	}

	if (const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		//BeginPlay 전 변화는 클라이언트 최초 복제(기본값 → 서버 값)라 표시하지 않음
		AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ASC->GetAvatarActor());

		if (Enemy && Enemy->HasActorBegunPlay())
		{
			Enemy->ShowEnemyHealthBar();
		}
	}
}

// Rep Notify Functions
void UEnemyAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	Super::OnRep_Health(OldHealth);

	//클라이언트: Health 복제 수신 시 HP바 표시 (서버는 PostGameplayEffectExecute에서 처리)
	//REPNOTIFY_Always라 값이 같아도 호출되므로 실제 변화가 있을 때만
	if (!FMath::IsNearlyEqual(OldHealth.GetCurrentValue(), GetHealth()))
	{
		ShowHealthBarOnHealthChanged();
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
