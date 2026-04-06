#include "Notifies/AnimNotifyState_Lunging.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Characters/EnemyCharacter.h"
#include "Components/CapsuleComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogANS_Lunging, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogANS_Lunging, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UAnimNotifyState_Lunging::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AActor* Owner = MeshComp->GetOwner();
	UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC) return;

	FGameplayEventData EventData;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyLungeStartTag();
	EventData.EventMagnitude = TotalDuration; //ANS 지속시간 = 이동 시간

	ASC->HandleGameplayEvent(EventData.EventTag, &EventData);

	//Lunge 중 적-플레이어 겹침 허용
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Owner);
	if (Enemy)
	{
		UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		if (Capsule)
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		}
	}

	DEBUG_LOG(TEXT("Lunging Begin — Duration: %.2f"), TotalDuration);
}

void UAnimNotifyState_Lunging::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AActor* Owner = MeshComp->GetOwner();
	UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC) return;

	FGameplayEventData EventData;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyLungeEndTag();

	ASC->HandleGameplayEvent(EventData.EventTag, &EventData);

	//콜리전 복구
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Owner);
	if (Enemy)
	{
		UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		if (Capsule)
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
	}

	DEBUG_LOG(TEXT("Lunging End"));
}
