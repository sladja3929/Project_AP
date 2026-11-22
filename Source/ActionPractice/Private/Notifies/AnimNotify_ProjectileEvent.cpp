#include "Notifies/AnimNotify_ProjectileEvent.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAnimNotify_ProjectileEvent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAnimNotify_ProjectileEvent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UAnimNotify_ProjectileEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;

	if (!IsValid(Owner))
	{
		return;
	}

	UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();

	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayTag EventTag;

	switch (EventType)
	{
	case EProjectileNotifyEvent::Spawn:
		EventTag = UGameplayTagsSubsystem::GetEventNotifyProjectileSpawnTag();
		break;
	case EProjectileNotifyEvent::Grab:
		EventTag = UGameplayTagsSubsystem::GetEventNotifyProjectileGrabTag();
		break;
	case EProjectileNotifyEvent::Launch:
		EventTag = UGameplayTagsSubsystem::GetEventNotifyProjectileLaunchTag();
		break;
	}

	FGameplayEventData EventData;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.EventTag = EventTag;

	ASC->HandleGameplayEvent(EventTag, &EventData);
	DEBUG_LOG(TEXT("Projectile Event AN: %s"), *EventTag.ToString());
}

FString UAnimNotify_ProjectileEvent::GetNotifyName_Implementation() const
{
	switch (EventType)
	{
	case EProjectileNotifyEvent::Spawn:
		return TEXT("Projectile Spawn");
	case EProjectileNotifyEvent::Grab:
		return TEXT("Projectile Grab");
	case EProjectileNotifyEvent::Launch:
		return TEXT("Projectile Launch");
	}

	return TEXT("Projectile Event");
}
