#include "AI/EnemyAIController.h"
#include "AI/StateTree/GASStateTreeAIComponent.h"
#include "AI/EnemyPack.h"
#include "AI/EnemyPackSubsystem.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/BossCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "TimerManager.h"
#include "Navigation/PathFollowingComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAIController, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAIController, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

AEnemyAIController::AEnemyAIController()
{
	//GASStateTree Component 생성
	GASStateTreeAIComponent = CreateDefaultSubobject<UGASStateTreeAIComponent>(TEXT("GASStateTreeAIComponent"));
	check(GASStateTreeAIComponent);

	BrainComponent = GASStateTreeAIComponent;
	
	//AI Perception Component 생성, 부모 클래스에 멤버변수는 있지만 생성은 자식에서 직접 해야 함
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent")));

	//Sight Config 기본 설정
	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (SightConfig)
	{
		//시야 범위 설정
		SightConfig->SightRadius = 3000.0f;				//시야 반경 (cm)
		SightConfig->LoseSightRadius = 3500.0f;			//시야 상실 반경 (추적 유지)
		SightConfig->PeripheralVisionAngleDegrees = 60.0f;	//주변 시야 각도 (좌우 각 60도 = 총 120도). 인지 단계에만 적용, 전투 중에는 무시

		//Perception Component에 Sight Config 추가
		GetPerceptionComponent()->ConfigureSense(*SightConfig);
		GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	//Perception 이벤트 바인딩
	GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);

	//AI 로직 자동 시작
	bStartAILogicOnPossess = true;

	//EnvQuery 등을 위한 Pawn 부착
	bAttachToPawn = true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	DEBUG_LOG(TEXT("OnPossess: InPawn=%s"), *GetNameSafe(InPawn));

	EnemyCharacter = Cast<AEnemyCharacter>(InPawn);
	if (!EnemyCharacter.IsValid())
	{
		DEBUG_LOG(TEXT("OnPossess: Failed to cast Pawn to EnemyCharacter"));
		return;
	}

	//보스는 청각 없이 시야와 보스룸 진입(ABossArena → PerceiveTarget)으로 인지
	if (!InPawn->IsA<ABossCharacter>())
	{
		ConfigureHearing();
	}

	if (!GASStateTreeAIComponent)
	{
		DEBUG_LOG(TEXT("OnPossess: GASStateTreeAIComponent is nullptr"));
		return;
	}

	DEBUG_LOG(TEXT("OnPossess: Restarting StateTree logic"));
	GASStateTreeAIComponent->RestartLogic();
}

void AEnemyAIController::ConfigureHearing()
{
	UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();

	//재빙의 시 중복 구성 방지
	if (!PerceptionComp || PerceptionComp->GetSenseConfig<UAISenseConfig_Hearing>())
	{
		return;
	}

	UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->SetMaxAge(HearingMaxAge);

	//팀 인터페이스 미사용이라 플레이어는 Neutral. 시야와 같이 전부 허용 (엔진 기본값은 전부 false라 소리가 걸러짐)
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	PerceptionComp->ConfigureSense(*HearingConfig);

	//런타임 ConfigureSense는 컴포넌트 필터만 갱신하고 Perception System에 캐시된 리스너 필터는 그대로라
	//청각이 이 리스너를 "청각 없음"으로 보고 건너뜀. 리스너 갱신으로 필터를 다시 복사하고 청각 설정을 등록
	PerceptionComp->RequestStimuliListenerUpdate();
	DEBUG_LOG(TEXT("ConfigureHearing: Range=%.0f MaxAge=%.1f"), HearingRange, HearingMaxAge);
}

void AEnemyAIController::OnUnPossess()
{
	DEBUG_LOG(TEXT("OnUnPossess: Called. Pawn=%s"), *GetNameSafe(GetPawn()));

	if (GASStateTreeAIComponent)
	{
		DEBUG_LOG(TEXT("OnUnPossess: Stopping StateTree logic"));
		GASStateTreeAIComponent->StopLogic("Unpossessed");
	}

	//폰이 사라지는 시점이라 알림 없이 정리
	StopReturnHome(false);
	GetWorldTimerManager().ClearTimer(CombatReleaseCheckTimer);
	bInCombat = false;
	CurrentTarget.Reset();
	LeavePack();

	EnemyCharacter.Reset();

	Super::OnUnPossess();
}

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	const bool bSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
	const bool bHearing = Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>();

	if (!Actor || (!bSight && !bHearing))
	{
		return;
	}

	AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(Actor);

	if (!Player)
	{
		return;
	}

	//전투 중에는 시야 변화와 무관하게 타깃 유지 (해제는 CheckCombatRelease 담당)
	//귀환 중 재인지는 거리 조건이 있어 이벤트가 아닌 귀환 타이머(CheckReturnHome)가 판정
	if (bInCombat || bReturningHome)
	{
		return;
	}

	//사망한 적은 인지하지 않음 (리셋 시 ResetDeathHandled 후 ResetAwareness로 재감지)
	if (IsOwnerDeathHandled())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		//먼저 인지한 플레이어 유지 (소리로 인지해도 같은 타깃 인지 상태. 돌아보기·접근은 ST가 담당)
		if (!CurrentTarget.IsValid() && IsAlivePlayer(Player))
		{
			CurrentTarget.Actor = Player;
			DEBUG_LOG(TEXT("Target Perceived (%s): %s"), bSight ? TEXT("Sight") : TEXT("Hearing"), *Actor->GetName());
		}

		return;
	}

	//시야 상실 또는 청각 만료
	if (CurrentTarget.Actor.Get() == Player)
	{
		//다른 감각으로 아직 인지 중이면 유지 (시야 밖이어도 최근 소리가 남아 있으면 추적)
		const UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();

		if (PerceptionComp && PerceptionComp->HasAnyCurrentStimulus(*Player))
		{
			return;
		}

		CurrentTarget.Reset();
		DEBUG_LOG(TEXT("Target Lost: %s"), *Actor->GetName());
		//시야·청각으로 인지 중인 다른 플레이어가 있으면 교체
		if (AActor* OtherPlayer = FindPerceivedPlayer())
		{
			CurrentTarget.Actor = OtherPlayer;
		}
	}
}

#pragma region "Combat State"

void AEnemyAIController::PerceiveTarget(AActor* Target)
{
	//OnTargetPerceptionUpdated와 같은 기준: 전투 중이면 기존 타깃 유지, 사망한 적은 인지 안 함, 먼저 인지한 플레이어 유지
	if (bInCombat || IsOwnerDeathHandled() || CurrentTarget.IsValid() || !IsAlivePlayer(Target))
	{
		return;
	}

	//시야 차단(CanBeSeenFrom)과 같은 기준으로 State.Undetectable 대상은 인지하지 않음
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const FGameplayTag& UndetectableTag = UGameplayTagsSubsystem::GetStateUndetectableTag();

	if (TargetASC && UndetectableTag.IsValid() && TargetASC->HasMatchingGameplayTag(UndetectableTag))
	{
		return;
	}

	CurrentTarget.Actor = Target;
	DEBUG_LOG(TEXT("PerceiveTarget: %s"), *GetNameSafe(Target));

	//귀환 중 보스룸 진입은 즉시 재인지 (타깃 설정 후 StateTree 재시작)
	if (bReturningHome)
	{
		ReengageFromReturnHome();
	}
}

void AEnemyAIController::EnterCombat(AActor* Target)
{
	if (bInCombat || !IsAlivePlayer(Target))
	{
		return;
	}

	//인지 중이던 대상과 다르면(시야 밖에서 피격 등) 교체. 거리·각도는 다음 Evaluator 틱에 갱신
	if (CurrentTarget.Actor.Get() != Target)
	{
		CurrentTarget.Reset();
		CurrentTarget.Actor = Target;
	}

	bInCombat = true;
	GetWorldTimerManager().SetTimer(CombatReleaseCheckTimer, this, &AEnemyAIController::CheckCombatRelease, CombatReleaseCheckInterval, true);

	//귀환 중 피격으로 전투에 들어오면 귀환을 끝내고 StateTree 재시작 (전투 상태 설정 후라 바로 Combat 선택)
	if (bReturningHome)
	{
		ReengageFromReturnHome();
	}

	DEBUG_LOG(TEXT("EnterCombat: %s"), *GetNameSafe(Target));
	OnCombatStateChanged.Broadcast(true, Target);

	//무리 소집 (진입 경로와 무관: Howl·근거리 진입·피격 모두. 무리 미사용 적이거나 이미 소속이면 무시)
	if (UEnemyPackSubsystem* PackSubsystem = UWorld::GetSubsystem<UEnemyPackSubsystem>(GetWorld()))
	{
		PackSubsystem->RallyPack(this, Target);
	}
}

void AEnemyAIController::ExitCombat()
{
	if (!bInCombat)
	{
		return;
	}

	bInCombat = false;
	GetWorldTimerManager().ClearTimer(CombatReleaseCheckTimer);
	LeavePack();

	AActor* PreviousTarget = CurrentTarget.Actor.Get();
	AActor* PerceivedPlayer = FindPerceivedPlayer();

	if (!PerceivedPlayer)
	{
		CurrentTarget.Reset();
	}
	else if (PerceivedPlayer != PreviousTarget)
	{
		CurrentTarget.Reset();
		CurrentTarget.Actor = PerceivedPlayer;
	}

	DEBUG_LOG(TEXT("ExitCombat: %s (Perceived=%s)"), *GetNameSafe(PreviousTarget), *GetNameSafe(PerceivedPlayer));
	OnCombatStateChanged.Broadcast(false, PreviousTarget);
}

void AEnemyAIController::ResetAwareness()
{
	const bool bWasInCombat = bInCombat;
	AActor* PreviousTarget = CurrentTarget.Actor.Get();

	//리셋은 RestartEnemyAI가 StateTree를 재시작하므로 귀환만 정리
	StopReturnHome(false);
	bInCombat = false;
	GetWorldTimerManager().ClearTimer(CombatReleaseCheckTimer);
	CurrentTarget.Reset();
	LeavePack();

	//이미 감지된 대상은 감지 이벤트가 다시 오지 않으므로 인지 기록 삭제 (다음 시야 판정에서 새로 감지)
	if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
	{
		PerceptionComp->ForgetAll();
	}

	if (bWasInCombat)
	{
		OnCombatStateChanged.Broadcast(false, PreviousTarget);
	}
}

void AEnemyAIController::ClearAwarenessOnDeath()
{
	StopReturnHome(false);
	bInCombat = false;
	GetWorldTimerManager().ClearTimer(CombatReleaseCheckTimer);
	CurrentTarget.Reset();
	LeavePack();
}

void AEnemyAIController::CheckCombatRelease()
{
	AActor* Target = CurrentTarget.Actor.Get();
	const APawn* ControlledPawn = GetPawn();

	if (!ControlledPawn || !IsAlivePlayer(Target))
	{
		ExitCombat();
		return;
	}

	//전투 구역(보스룸) 안에 있는 동안은 거리·레시로 해제하지 않음 (보스룸이 LoseSightRadius보다 넓어 입구에서 진입 즉시 해제되는 것 방지)
	if (EnemyCharacter.IsValid() && EnemyCharacter->IsTargetInCombatArea(Target))
	{
		return;
	}

	//레시: 스폰 위치에서 너무 멀어지면 타깃을 잃고 귀환 (무리 여부와 무관하게 개별 판정, 귀환 중 재교전 직후 유예 시간 동안은 보류)
	if (LeashDistance > 0.0f && EnemyCharacter.IsValid() && GetWorld()->GetTimeSeconds() >= LeashSuspendedUntil
		&& FVector::Dist2D(ControlledPawn->GetActorLocation(), EnemyCharacter->SpawnLocation) > LeashDistance)
	{
		StartReturnHome();
		return;
	}

	//시야 상실 반경을 전투 이탈 거리로 사용 (수평 거리, UpdateTargetInfoEvaluator와 같은 기준)
	const UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();
	const UAISenseConfig_Sight* SightConfig = PerceptionComp ? PerceptionComp->GetSenseConfig<UAISenseConfig_Sight>() : nullptr;
	const float ReleaseDistance = SightConfig ? SightConfig->LoseSightRadius : 0.0f;

	if (ReleaseDistance <= 0.0f)
	{
		return;
	}

	//무리 소속이면 구성원 전원이 해제 거리 밖일 때만 해제 (혼자 멀어진 구성원은 무리 타깃을 유지해 다시 따라옴)
	const UEnemyPack* CurrentPack = Pack.Get();
	const bool bBeyondReleaseDistance = CurrentPack
		? CurrentPack->AreAllMembersBeyond(ReleaseDistance)
		: FVector::Dist2D(ControlledPawn->GetActorLocation(), Target->GetActorLocation()) > ReleaseDistance;

	if (bBeyondReleaseDistance)
	{
		ExitCombat();
	}
}

void AEnemyAIController::StartReturnHome()
{
	const AEnemyCharacter* Enemy = EnemyCharacter.Get();

	if (!Enemy || bReturningHome)
	{
		return;
	}

	//StateTree가 배회·추격 이동으로 덮어쓰지 않도록 귀환 동안 정지 (재인지·도착 시 재시작)
	//전투 해제 알림보다 먼저 멈춰, 알림 리스너가 정지 직전의 ST에 영향을 주지 않게 함 (ExitState가 추격 속도 GE도 제거)
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("ReturnHome"));
	}

	//전투 해제 알림(조우 연출 종료 등)은 ExitCombat이 담당. 시야 안 플레이어로 바꿔 잡은 타깃도 귀환 동안은 해제
	ExitCombat();
	CurrentTarget.Reset();

	bReturningHome = true;
	ReturnHomeStartTime = GetWorld()->GetTimeSeconds();

	StopMovement();
	RequestMoveHome();

	GetWorldTimerManager().SetTimer(ReturnHomeCheckTimer, this, &AEnemyAIController::CheckReturnHome, CombatReleaseCheckInterval, true);
	DEBUG_LOG(TEXT("StartReturnHome: %s -> %s"), *GetNameSafe(GetPawn()), *Enemy->SpawnLocation.ToString());
}

void AEnemyAIController::CheckReturnHome()
{
	const APawn* ControlledPawn = GetPawn();
	const AEnemyCharacter* Enemy = EnemyCharacter.Get();

	if (!ControlledPawn || !Enemy)
	{
		StopReturnHome(false);
		return;
	}

	const float DistanceFromHome = FVector::Dist2D(ControlledPawn->GetActorLocation(), Enemy->SpawnLocation);

	//레시 경계에서 추격·귀환이 반복되지 않도록 안쪽 여유 거리 안에서만 시야·청각 재인지
	if (DistanceFromHome <= LeashDistance - LeashReacquireMargin)
	{
		if (AActor* PerceivedPlayer = FindPerceivedPlayer())
		{
			CurrentTarget.Actor = PerceivedPlayer;
			DEBUG_LOG(TEXT("CheckReturnHome: Reacquired %s"), *GetNameSafe(PerceivedPlayer));
			StopReturnHome(true);
			return;
		}
	}

	bool bArrived = DistanceFromHome <= ReturnAcceptanceRadius;
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - ReturnHomeStartTime >= ReturnHomeTimeout;

	//이동이 도중에 끝났으면(피격 경직·회피 등으로 중단) 다시 요청. 경로가 없으면 귀환 포기
	const UPathFollowingComponent* PathComp = GetPathFollowingComponent();

	if (!bArrived && !bTimedOut && (!PathComp || PathComp->GetStatus() == EPathFollowingStatus::Idle))
	{
		const EPathFollowingRequestResult::Type Result = RequestMoveHome();

		if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			bArrived = true;
		}
		else if (Result == EPathFollowingRequestResult::Failed)
		{
			//인지 기록은 유지 (지우면 레시 밖에서 보이는 플레이어를 곧바로 다시 감지해 추격·귀환 반복)
			DEBUG_LOG(TEXT("CheckReturnHome: No path home, give up"));
			StopReturnHome(true);
			return;
		}
	}

	if (bArrived || bTimedOut)
	{
		DEBUG_LOG(TEXT("CheckReturnHome: Finished (Arrived=%d TimedOut=%d)"), bArrived, bTimedOut);

		//도착했을 때만 인지 기록 삭제 (이미 감지된 대상은 이벤트가 다시 오지 않으므로, 자리에 돌아온 뒤 보이는 플레이어를 새로 감지)
		//시간 초과는 레시 밖일 수 있어 유지
		if (bArrived)
		{
			if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
			{
				PerceptionComp->ForgetAll();
			}
		}

		StopReturnHome(true);
	}
}

void AEnemyAIController::ReengageFromReturnHome()
{
	//레시 밖에서 재교전해도 유예 시간 동안은 다시 귀환하지 않음
	LeashSuspendedUntil = GetWorld()->GetTimeSeconds() + LeashReengageGrace;
	StopReturnHome(true);
}

EPathFollowingRequestResult::Type AEnemyAIController::RequestMoveHome()
{
	const AEnemyCharacter* Enemy = EnemyCharacter.Get();

	if (!Enemy)
	{
		return EPathFollowingRequestResult::Failed;
	}

	return MoveToLocation(Enemy->SpawnLocation, ReturnAcceptanceRadius, true, true, true, false);
}

void AEnemyAIController::StopReturnHome(bool bRestartLogic)
{
	if (!bReturningHome)
	{
		return;
	}

	bReturningHome = false;
	GetWorldTimerManager().ClearTimer(ReturnHomeCheckTimer);
	StopMovement();

	//사망한 적은 StateTree를 다시 돌리지 않음 (리셋 시 RestartEnemyAI가 재시작)
	if (bRestartLogic && BrainComponent && !IsOwnerDeathHandled())
	{
		BrainComponent->RestartLogic();
	}
}

bool AEnemyAIController::IsPackAttacker() const
{
	const UEnemyPack* CurrentPack = Pack.Get();
	return !CurrentPack || CurrentPack->IsAttacker(this);
}

void AEnemyAIController::SetPack(UEnemyPack* InPack)
{
	Pack = InPack;
}

void AEnemyAIController::LeavePack()
{
	if (!Pack.IsValid())
	{
		Pack.Reset();
		return;
	}

	if (UEnemyPackSubsystem* PackSubsystem = UWorld::GetSubsystem<UEnemyPackSubsystem>(GetWorld()))
	{
		PackSubsystem->LeavePack(this);
	}

	Pack.Reset();
}

AActor* AEnemyAIController::FindPerceivedPlayer() const
{
	const UAIPerceptionComponent* PerceptionComp = GetAIPerceptionComponent();

	if (!PerceptionComp)
	{
		return nullptr;
	}

	//감각 무관 (만료되지 않은 시야·청각 자극이 하나라도 있는 대상)
	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

	for (AActor* PerceivedActor : PerceivedActors)
	{
		if (IsAlivePlayer(PerceivedActor))
		{
			return PerceivedActor;
		}
	}

	return nullptr;
}

bool AEnemyAIController::IsAlivePlayer(const AActor* Actor)
{
	if (!Cast<AActionPracticeCharacter>(Actor))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	const FGameplayTag& StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();

	return !(ASC && StateDeadTag.IsValid() && ASC->HasMatchingGameplayTag(StateDeadTag));
}

bool AEnemyAIController::IsOwnerDeathHandled() const
{
	const UBaseAbilitySystemComponent* OwnASC = EnemyCharacter.IsValid() ? Cast<UBaseAbilitySystemComponent>(EnemyCharacter->GetAbilitySystemComponent()) : nullptr;
	return OwnASC && OwnASC->IsDeathHandled();
}

#pragma endregion