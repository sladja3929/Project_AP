#include "Notifies/AnimNotifyState_HitDetection.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagAssetInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayTagsDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "ProfilingDebugging/MiscTrace.h"

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
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    DEBUG_LOG(TEXT("HitDetection NotifyBegin: Anim=%s, Duration=%.3f"),
        *GetNameSafe(Animation), TotalDuration);

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

    // HitDetectionStart 이벤트 송신
    FGameplayEventData EventData;
    EventData.Instigator = Owner;
    EventData.Target = Owner;
    EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag();

    // Duration을 EventMagnitude에 저장
    EventData.EventMagnitude = TotalDuration;

    //두 히트디텍션 모드(AttackTrace/CapsuleOverlap)가 공유하는 단일 지점 — 모드 무관하게 동일 윈도우가 마킹됨
    //가드를 통과해 실제 이벤트를 보내는 지점에 둬야 컴포넌트 로그와 1:1 (블렌드 전환의 헛발 NotifyBegin은 위에서 걸러짐)
    //Insights에서 이 구간을 선택해 씬쿼리 타이머를 비교 (캡처 시 -trace=...,bookmark 필요)
    TRACE_BOOKMARK(TEXT("HitDetectionWindow_Begin"));

    ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag(), &EventData);
}

void UAnimNotifyState_HitDetection::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    DEBUG_LOG(TEXT("HitDetection NotifyEnd: Anim=%s"),
        *GetNameSafe(Animation));

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

    //Begin과 대칭 — 가드 통과 후 실제 이벤트 송신 지점에 마킹 (헛발 NotifyEnd 배제)
    TRACE_BOOKMARK(TEXT("HitDetectionWindow_End"));

    ASC->HandleGameplayEvent(UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag(), &EventData);
}