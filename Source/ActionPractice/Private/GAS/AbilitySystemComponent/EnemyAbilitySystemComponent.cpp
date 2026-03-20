#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Characters/EnemyCharacter.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Characters/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "Items/AttackData.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAbilitySystemComponent, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAbilitySystemComponent, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UEnemyAbilitySystemComponent::UEnemyAbilitySystemComponent()
{
	//적은 Minimal mode 사용 (서버 권한, GameplayEffect만 복제)
	SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void UEnemyAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CachedEnemyCharacter = Cast<AEnemyCharacter>(Owner);
	}

	//적 전용 HitReaction 태그로 덮어쓰기 (BaseASC::HandleOnDamagedResolved가 이 태그로 어빌리티 검색)
	AbilityHitReactionTag = UGameplayTagsSubsystem::GetEnemyAbilityHitReactionTag();

	//그로기 태그 초기화
	EnemyAbilityGroggyTag = UGameplayTagsSubsystem::GetEnemyAbilityGroggyTag();
}

void UEnemyAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	CachedEnemyCharacter = Cast<AEnemyCharacter>(InOwnerActor);
}

const UEnemyAttributeSet* UEnemyAbilitySystemComponent::GetEnemyAttributeSet() const
{
	return this->GetSet<UEnemyAttributeSet>();
}

void UEnemyAbilitySystemComponent::CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	//BaseASC의 기본 계산 (HP, Poise 적용)
	Super::CalculateAndSetAttributes(SourceActor, FinalAttackData);

	//Poise 대미지의 일정 비율을 Stamina(그로기 게이지)에도 적용
	if (FinalAttackData.PoiseDamage > 0.0f)
	{
		UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
		UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
		if (BaseAttributeSet)
		{
			const float GroggyDamage = FinalAttackData.PoiseDamage * GroggyDamageRate;
			const float OldStamina = BaseAttributeSet->GetStamina();
			const float NewStamina = FMath::Max(0.0f, OldStamina - GroggyDamage);
			BaseAttributeSet->SetStamina(NewStamina);

			DEBUG_LOG(TEXT("EnemyASC Groggy: PoiseDmg=%.1f, Rate=%.1f, GroggyDmg=%.1f, Stamina=%.1f->%.1f"),
				FinalAttackData.PoiseDamage, GroggyDamageRate, GroggyDamage, OldStamina, NewStamina);
		}
	}
}

void UEnemyAbilitySystemComponent::HandleOnDamagedResolved(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	if (!CachedCharacter.IsValid())
	{
		return;
	}

	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (!BaseAttributeSet)
	{
		return;
	}

	//1. 사망 체크 (최우선)
	if (BaseAttributeSet->GetHealth() <= 0.0f)
	{
		DEBUG_LOG(TEXT("HandleOnDamagedResolved: Enemy died"));
		HandleDeath();
		return;
	}

	//2. 그로기 체크 (HitReaction보다 우선)
	if (ShouldActivateGroggy())
	{
		DEBUG_LOG(TEXT("HandleOnDamagedResolved: Groggy triggered"));
		ActivateGroggyAbility(FinalAttackData);
		return;
	}

	//3. HitReaction 체크 (Poise ≤ 0)
	if (ShouldActivateHitReaction())
	{
		if (AbilityHitReactionTag.IsValid())
		{
			TArray<FGameplayAbilitySpec*> HitReactionSpecs;
			GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityHitReactionTag), HitReactionSpecs);

			if (HitReactionSpecs.Num() > 0)
			{
				FGameplayEventData EventData;
				PrepareHitReactionEventData(EventData, FinalAttackData);

				if (TryActivateAbilityWithEventData(HitReactionSpecs[0]->Handle, &EventData))
				{
					DEBUG_LOG(TEXT("EnemyHitReaction activated with Poise=%.1f"), EventData.EventMagnitude);
				}
				else
				{
					DEBUG_LOG(TEXT("EnemyHitReaction activation failed"));
				}
			}
		}
	}
}

bool UEnemyAbilitySystemComponent::ShouldActivateGroggy() const
{
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	const UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (!BaseAttributeSet)
	{
		return false;
	}

	return BaseAttributeSet->GetStamina() <= 0.0f;
}

void UEnemyAbilitySystemComponent::ActivateGroggyAbility(const FFinalAttackData& FinalAttackData)
{
	if (!EnemyAbilityGroggyTag.IsValid())
	{
		DEBUG_LOG(TEXT("ActivateGroggyAbility: GroggyTag is not valid"));
		return;
	}

	TArray<FGameplayAbilitySpec*> GroggySpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(EnemyAbilityGroggyTag), GroggySpecs);

	if (GroggySpecs.IsEmpty())
	{
		DEBUG_LOG(TEXT("ActivateGroggyAbility: No Groggy ability spec found"));
		return;
	}

	//이미 활성화 중이면 중복 방지
	if (GroggySpecs[0]->IsActive())
	{
		DEBUG_LOG(TEXT("ActivateGroggyAbility: Already active"));
		return;
	}

	if (!TryActivateAbilityWithEventData(GroggySpecs[0]->Handle, nullptr))
	{
		DEBUG_LOG(TEXT("ActivateGroggyAbility: Failed to activate"));
	}
}

void UEnemyAbilitySystemComponent::HandleDeath()
{
	if (bDeathHandled) return;
	Super::HandleDeath();

	if (!CachedEnemyCharacter.IsValid()) return;

	//서버 권한에서만 Death Ability 활성화
	if (!CachedEnemyCharacter->HasAuthority()) return;

	//EnemyAbility.Death 태그로 스펙 조회
	const FGameplayTag AbilityDeathTag = UGameplayTagsSubsystem::GetEnemyAbilityDeathTag();
	if (!AbilityDeathTag.IsValid())
	{
		DEBUG_LOG(TEXT("EnemyASC::HandleDeath - AbilityDeathTag is not valid"));
		return;
	}

	TArray<FGameplayAbilitySpec*> DeathSpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityDeathTag), DeathSpecs);
	if (DeathSpecs.IsEmpty())
	{
		DEBUG_LOG(TEXT("EnemyASC::HandleDeath - No Death ability spec found"));
		return;
	}

	//이미 활성화 중이면 중복 방지
	if (DeathSpecs[0]->IsActive())
	{
		DEBUG_LOG(TEXT("EnemyASC::HandleDeath - EnemyDeathAbility already active"));
		return;
	}

	if (!TryActivateAbilityWithEventData(DeathSpecs[0]->Handle, nullptr))
	{
		DEBUG_LOG(TEXT("EnemyASC::HandleDeath - Failed to activate EnemyDeathAbility"));
	}
}
