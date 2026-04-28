#include "Notifies/AnimNotifyState_TrackingTarget.h"
#include "AbilitySystemComponent.h"
#include "Characters/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogANS_TrackingTarget, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogANS_TrackingTarget, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UAnimNotifyState_TrackingTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	DEBUG_LOG(TEXT("TrackingTarget Begin — Duration: %.2f"), TotalDuration);
}

void UAnimNotifyState_TrackingTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy) return;

	AEnemyAIController* AIController = Enemy->GetEnemyAIController();
	if (!AIController) return;

	const FCurrentTarget& TargetInfo = AIController->GetCurrentTarget();
	if (!TargetInfo.IsValid() || !TargetInfo.Actor.IsValid()) return;

	AActor* TargetActor = TargetInfo.Actor.Get();

	//타겟 방향으로 회전
	Enemy->RotateToTarget(TargetActor, RotateInterpTime);

	//ASC 이벤트로 타겟 액터를 전달 → 어빌리티가 위치를 캐싱
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	if (!ASC) return;

	FGameplayEventData EventData;
	EventData.Instigator = Enemy;
	EventData.Target = TargetActor;
	EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyTrackingTargetTag();

	ASC->HandleGameplayEvent(EventData.EventTag, &EventData);
}

void UAnimNotifyState_TrackingTarget::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	DEBUG_LOG(TEXT("TrackingTarget End"));
}
