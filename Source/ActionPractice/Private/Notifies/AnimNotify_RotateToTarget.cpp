#include "Notifies/AnimNotify_RotateToTarget.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAnimNotify_RotateToTarget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAnimNotify_RotateToTarget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UAnimNotify_RotateToTarget::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !MeshComp->GetOwner())
		return;

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
		return;

	UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC || !IsValid(ASC))
		return;

	FGameplayEventData EventData;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTag();

	ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyRotateToTargetTag(), &EventData);
	DEBUG_LOG(TEXT("RotateToTarget AN"));
}