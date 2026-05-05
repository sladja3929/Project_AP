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

namespace
{
	//Begin/End 시점에 destination 이벤트 1회 발행 (Tick에서 매 틱 보내던 것을 분리)
	//서버 권한일 때만 호출되어야 하므로 호출자 측에서 가드한 뒤 진입할 것
	void DispatchTrackingTargetEvent(AEnemyCharacter* Enemy)
	{
		if (!Enemy) return;

		AEnemyAIController* AIController = Enemy->GetEnemyAIController();
		if (!AIController) return;

		const FCurrentTarget& TargetInfo = AIController->GetCurrentTarget();
		if (!TargetInfo.IsValid() || !TargetInfo.Actor.IsValid()) return;

		AActor* TargetActor = TargetInfo.Actor.Get();

		UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
		if (!ASC) return;

		FGameplayEventData EventData;
		EventData.Instigator = Enemy;
		EventData.Target = TargetActor;
		EventData.EventTag = UGameplayTagsSubsystem::GetEventNotifyTrackingTargetTag();

		ASC->HandleGameplayEvent(EventData.EventTag, &EventData);
	}
}

void UAnimNotifyState_TrackingTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	DEBUG_LOG(TEXT("TrackingTarget Begin — Duration: %.2f"), TotalDuration);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy) return;

	//서버 권한에서만 destination 이벤트 발행 (ServerOnly 어빌리티의 핸들러가 받음)
	//시뮬레이트 프록시에서는 어빌리티가 활성화되지 않으므로 이벤트가 무의미함
	if (!Enemy->HasAuthority()) return;

	DispatchTrackingTargetEvent(Enemy);
}

void UAnimNotifyState_TrackingTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy) return;

	//시뮬레이트 프록시에서 회전을 직접 보간하면 서버 복제와 conflict하여 jitter 발생
	if (!Enemy->HasAuthority()) return;

	AEnemyAIController* AIController = Enemy->GetEnemyAIController();
	if (!AIController) return;

	const FCurrentTarget& TargetInfo = AIController->GetCurrentTarget();
	if (!TargetInfo.IsValid() || !TargetInfo.Actor.IsValid()) return;

	AActor* TargetActor = TargetInfo.Actor.Get();

	//매 틱 회전 보간만 유지 (destination 이벤트는 Begin/End에서만 발행)
	Enemy->RotateToTarget(TargetActor, RotateInterpTime);
}

void UAnimNotifyState_TrackingTarget::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	DEBUG_LOG(TEXT("TrackingTarget End"));

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy) return;

	//서버 권한에서만 destination 최종 갱신 (Lunging ANS와 겹친 배치라면 진행 중인 RootMotion target을 1회 갱신)
	if (!Enemy->HasAuthority()) return;

	DispatchTrackingTargetEvent(Enemy);
}
