#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Characters/EnemyCharacter.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "AI/EnemyAIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	//이동 정지
	if (UCharacterMovementComponent* MoveComp = CachedEnemyCharacter->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	//AI 로직 정지
	if (AEnemyAIController* AIController = CachedEnemyCharacter->GetEnemyAIController())
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Death"));
		}
	}

	DEBUG_LOG(TEXT("EnemyASC::HandleDeath - Movement and AI stopped"));
}
