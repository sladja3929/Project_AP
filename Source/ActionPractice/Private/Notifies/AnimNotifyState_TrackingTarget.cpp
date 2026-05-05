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
	//현재 타겟 actor와 위치를 안전하게 조회. 실패 시 false
	//timer 분기에서 위치 비교(거리 가드)에 활용하기 위해 발사 헬퍼와 분리
	bool ResolveCurrentTarget(AEnemyCharacter* Enemy, AActor*& OutTarget, FVector& OutLocation)
	{
		if (!Enemy) return false;

		AEnemyAIController* AIController = Enemy->GetEnemyAIController();
		if (!AIController) return false;

		const FCurrentTarget& TargetInfo = AIController->GetCurrentTarget();
		if (!TargetInfo.IsValid() || !TargetInfo.Actor.IsValid()) return false;

		OutTarget = TargetInfo.Actor.Get();
		if (!OutTarget) return false;

		OutLocation = OutTarget->GetActorLocation();
		return true;
	}

	//ASC로 destination 이벤트 발행
	//서버 권한일 때만 호출되어야 하므로 호출자 측에서 가드한 뒤 진입할 것
	void DispatchEventToASC(AEnemyCharacter* Enemy, AActor* TargetActor)
	{
		if (!Enemy || !TargetActor) return;

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

	AActor* TargetActor = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	if (!ResolveCurrentTarget(Enemy, TargetActor, TargetLocation)) return;

	//Begin 시점은 무조건 1회 발사 (lunge 시작점의 destination 권위)
	DispatchEventToASC(Enemy, TargetActor);

	//state 초기화 — 직전 lunge의 잔재가 남아있을 경우 덮어쓰기
	FTrackingState& State = ActiveStates.FindOrAdd(MeshComp);
	State.AccumulatedTime = 0.0f;
	State.LastDispatchedDestination = TargetLocation;
}

void UAnimNotifyState_TrackingTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy) return;

	//시뮬레이트 프록시에서 회전을 직접 보간하면 서버 복제와 conflict하여 jitter 발생
	if (!Enemy->HasAuthority()) return;

	AActor* TargetActor = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	if (!ResolveCurrentTarget(Enemy, TargetActor, TargetLocation)) return;

	//매 틱 회전 보간 유지
	Enemy->RotateToTarget(TargetActor, RotateInterpTime);

	//timer 비활성화 시 destination 이벤트는 Begin/End에서만 발사 (기존 동작 폴백)
	if (DispatchInterval <= 0.0f) return;

	FTrackingState& State = ActiveStates.FindOrAdd(MeshComp);
	State.AccumulatedTime += FrameDeltaTime;
	if (State.AccumulatedTime < DispatchInterval) return;

	//주기 도달 — drift 누적 방지를 위해 빼기 방식 사용 (리셋 X)
	State.AccumulatedTime -= DispatchInterval;

	//거리 가드: 마지막 발사 위치 대비 변화량이 임계값 미만이면 skip
	//→ 플레이어 정지 상태에서 무의미한 source 재생성 회피
	const float DistSquared = FVector::DistSquared(TargetLocation, State.LastDispatchedDestination);
	if (DistSquared < MinDispatchDistance * MinDispatchDistance) return;

	//발사 + 마지막 위치 갱신
	DispatchEventToASC(Enemy, TargetActor);
	State.LastDispatchedDestination = TargetLocation;
}

void UAnimNotifyState_TrackingTarget::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	DEBUG_LOG(TEXT("TrackingTarget End"));

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
	if (!Enemy) return;

	//state cleanup은 권한 무관 (시뮬프록시도 entry는 만들지 않으니 사실상 no-op이지만 안전 차원)
	ActiveStates.Remove(MeshComp);

	//서버 권한에서만 destination 최종 갱신 (Lunging ANS와 겹친 배치라면 진행 중인 RootMotion target을 1회 갱신)
	if (!Enemy->HasAuthority()) return;

	AActor* TargetActor = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	if (!ResolveCurrentTarget(Enemy, TargetActor, TargetLocation)) return;

	DispatchEventToASC(Enemy, TargetActor);
}
