#include "GAS/AbilitySystemComponent/BossAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Characters/BossCharacter.h"
#include "GAS/AttributeSet/BossAttributeSet.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossAbilitySystemComponent, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogBossAbilitySystemComponent, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UBossAbilitySystemComponent::UBossAbilitySystemComponent()
{
	// 보스는 Minimal mode 사용 (서버 권한, GameplayEffect만 복제)
	SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void UBossAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CachedBossCharacter = Cast<ABossCharacter>(Owner);
	}
}

void UBossAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	CachedBossCharacter = Cast<ABossCharacter>(InOwnerActor);
}

const UBossAttributeSet* UBossAbilitySystemComponent::GetBossAttributeSet() const
{
	return this->GetSet<UBossAttributeSet>();
}