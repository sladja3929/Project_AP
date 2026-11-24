#include "Notifies/AnimNotify_HitDetectionBurst.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAnimNotify_HitDetectionBurst, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAnimNotify_HitDetectionBurst, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UAnimNotify_HitDetectionBurst::UAnimNotify_HitDetectionBurst()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Orange;
#endif
}

void UAnimNotify_HitDetectionBurst::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !IsValid(MeshComp->GetOwner()))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();

	UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();

	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyHitDetectionBurstTag();

	DEBUG_LOG(TEXT("HitDetectionBurst: Owner=%s, Anim=%s"), *GetNameSafe(Owner), *GetNameSafe(Animation));

	ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyHitDetectionBurstTag(), &EventData);
}

FString UAnimNotify_HitDetectionBurst::GetNotifyName_Implementation() const
{
	return TEXT("HitDetectionBurst");
}
