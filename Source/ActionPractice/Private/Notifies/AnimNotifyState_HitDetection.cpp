#include "Notifies/AnimNotifyState_HitDetection.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagAssetInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayTagsDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAnimNotifyState_HitDetection, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAnimNotifyState_HitDetection, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UAnimNotifyState_HitDetection::UAnimNotifyState_HitDetection()
{
#if WITH_EDITOR
    NotifyColor = FColor::Orange;
#endif
}

void UAnimNotifyState_HitDetection::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp || !MeshComp->GetOwner())
        return;

    AActor* Owner = MeshComp->GetOwner();
    if (!IsValid(Owner))
        return;

    UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC || !IsValid(ASC))
        return;

    // AddCombo 이벤트 송신 (HitDetectionStart 이벤트 전에)
    FGameplayEventData AddComboEventData;
    AddComboEventData.Instigator = Owner;
    AddComboEventData.Target = Owner;
    AddComboEventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyAddComboTag();

    ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyAddComboTag(), &AddComboEventData);
    DEBUG_LOG(TEXT("HitDetection ANS: AddCombo"));

    // HitDetectionStart 이벤트 송신
    FGameplayEventData EventData;
    EventData.Instigator = Owner;
    EventData.Target = Owner;
    EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag();

    // Duration을 EventMagnitude에 저장
    EventData.EventMagnitude = TotalDuration;

    ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag(), &EventData);
    DEBUG_LOG(TEXT("HitDetection ANS: Start"));
}

void UAnimNotifyState_HitDetection::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
    EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag();

    ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag(), &EventData);
    DEBUG_LOG(TEXT("HitDetection ANS: End"));
}