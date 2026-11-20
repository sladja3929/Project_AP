#include "AI/EnemyAIController.h"
#include "AI/StateTree/GASStateTreeAIComponent.h"
#include "AI/EnemyPack.h"
#include "AI/EnemyPackSubsystem.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "TimerManager.h"

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

	if (!GASStateTreeAIComponent)
	{
		DEBUG_LOG(TEXT("OnPossess: GASStateTreeAIComponent is nullptr"));
		return;
	}

	DEBUG_LOG(TEXT("OnPossess: Restarting StateTree logic"));
	GASStateTreeAIComponent->RestartLogic();
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
	GetWorldTimerManager().ClearTimer(CombatReleaseCheckTimer);
	bInCombat = false;
	CurrentTarget.Reset();
	LeavePack();

	EnemyCharacter.Reset();

	Super::OnUnPossess();
}

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		return;
	}

	AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(Actor);

	if (!Player)
	{
		return;
	}

	//전투 중에는 시야 변화와 무관하게 타깃 유지 (해제는 CheckCombatRelease 담당)
	if (bInCombat)
	{
		return;
	}

	//사망한 적은 인지하지 않음 (리셋 시 ResetDeathHandled 후 ResetAwareness로 재감지)
	const UBaseAbilitySystemComponent* OwnASC = EnemyCharacter.IsValid() ? Cast<UBaseAbilitySystemComponent>(EnemyCharacter->GetAbilitySystemComponent()) : nullptr;

	if (OwnASC && OwnASC->IsDeathHandled())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		//먼저 인지한 플레이어 유지
		if (!CurrentTarget.IsValid() && IsAlivePlayer(Player))
		{
			CurrentTarget.Actor = Player;
			DEBUG_LOG(TEXT("Target Perceived: %s"), *Actor->GetName());
		}

		return;
	}

	if (CurrentTarget.Actor.Get() == Player)
	{
		CurrentTarget.Reset();
		DEBUG_LOG(TEXT("Target Lost: %s"), *Actor->GetName());

		//시야 안에 다른 플레이어가 있으면 교체
		if (AActor* OtherPlayer = FindPerceivedPlayer())
		{
			CurrentTarget.Actor = OtherPlayer;
		}
	}
}

#pragma region "Combat State"

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

	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

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

#pragma endregion