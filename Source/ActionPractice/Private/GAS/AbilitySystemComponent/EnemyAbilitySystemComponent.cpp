#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Characters/EnemyCharacter.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

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

void UEnemyAbilitySystemComponent::HandleDeath()
{
	if (bDeathHandled) return;
	Super::HandleDeath();

	if (!CachedEnemyCharacter.IsValid()) return;

	//서버 권한에서만 Death Ability 활성화
	if (!CachedEnemyCharacter->HasAuthority()) return;

	//AbilityDeath 태그로 스펙 조회
	const FGameplayTag AbilityDeathTag = UGameplayTagsSubsystem::GetAbilityDeathTag();
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
