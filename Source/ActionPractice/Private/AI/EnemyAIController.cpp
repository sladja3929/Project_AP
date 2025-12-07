#include "AI/EnemyAIController.h"
#include "AI/StateTree/GASStateTreeAIComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/BossCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossAIController, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBossAIController, Warning, Format, ##__VA_ARGS__)
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
		SightConfig->PeripheralVisionAngleDegrees = 90.0f;	//주변 시야 각도 (좌우 각 90도 = 총 180도)

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

	BossCharacter = Cast<ABossCharacter>(InPawn);
	if (!BossCharacter.IsValid())
	{
		DEBUG_LOG(TEXT("OnPossess: Failed to cast Pawn to BossCharacter"));
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

	BossCharacter.Reset();

	Super::OnUnPossess();
}

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	//시각 감지 케이스
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(Actor);

		if (Stimulus.WasSuccessfullySensed())
		{
			if (Player)
			{
				CurrentTarget.Actor = Player;
				DEBUG_LOG(TEXT("Target Detected: %s"), *Actor->GetName());
			}
		}
		else
		{
			if (Player)
			{
				CurrentTarget.Reset();
				DEBUG_LOG(TEXT("Target Lost: %s"), *Actor->GetName());
			}
		}
	}
}