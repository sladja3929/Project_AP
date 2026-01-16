#include "Characters/BossCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/AbilitySystemComponent/BossAbilitySystemComponent.h"
#include "GAS/AttributeSet/BossAttributeSet.h"
#include "AI/EnemyAIController.h"
#include "UI/BossHealthWidget.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Characters/HitDetection/EnemyAttackComponent.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossCharacter, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBossCharacter, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ABossCharacter::ABossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	//Controller Settings
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	//AI Controller Settings, AI Controller는 BP에서 할당
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	//Character Movement Settings
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	//GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CreateAbilitySystemComponent();
	CreateAttributeSet();

	//EnemyAttackComponent 생성
	EnemyAttackComponent = CreateDefaultSubobject<UEnemyAttackComponent>(TEXT("EnemyAttackComponent"));
}

TScriptInterface<IHitDetectionInterface> ABossCharacter::GetHitDetectionInterface() const
{
	return EnemyAttackComponent;
}

void ABossCharacter::CreateAbilitySystemComponent()
{
	AbilitySystemComponent = CreateDefaultSubobject<UBossAbilitySystemComponent>(TEXT("BossAbilitySystemComponent"));
}

void ABossCharacter::CreateAttributeSet()
{
	AttributeSet = CreateDefaultSubobject<UBossAttributeSet>(TEXT("BossAttributeSet"));
}

void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();

	DEBUG_LOG(TEXT("BossCharacter::BeginPlay: This=%p, ASC=%p, AttributeSet=%p"),
		this,
		AbilitySystemComponent.Get(),
		AttributeSet.Get());

	if (EnemyData)
	{
		DEBUG_LOG(TEXT("BossCharacter::BeginPlay: EnemyData=%s"), *GetNameSafe(EnemyData));
	}
	else
	{
		DEBUG_LOG(TEXT("BossCharacter::BeginPlay: EnemyData is nullptr"));
	}

	DEBUG_LOG(TEXT("BossCharacter::BeginPlay: StartAbilities count=%d"), StartAbilities.Num());
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartAbilities)
	{
		DEBUG_LOG(TEXT("  StartAbility: %s"), *GetNameSafe(AbilityClass));
	}

	//EnemyData의 모든 몽타주 프리로드
	if (EnemyData)
	{
		EnemyData->PreloadAllMontages();
	}

	// AI 관련 설정은 서버에서만 (싱글플레이어에서는 항상 true)
	if (HasAuthority())
	{
		//AIController의 Perception 델리게이트 바인딩
		AEnemyAIController* BossController = GetEnemyAIController();
		if (BossController)
		{
			UAIPerceptionComponent* PerceptionComponent = BossController->GetPerceptionComponent();
			if (PerceptionComponent)
			{
				PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ABossCharacter::OnPlayerDetected);
				DEBUG_LOG(TEXT("Perception delegate bound to BossCharacter"));
			}
		}
	}
}

void ABossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 보스는 복제할 추가 변수가 거의 없음
	// DetectedPlayer는 서버 전용, bHealthWidgetActive는 로컬 UI 상태
}

void ABossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveHealthWidget();

	Super::EndPlay(EndPlayReason);
}

void ABossCharacter::OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	//시각 감지 케이스
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(Actor);
		if (!Player) return;

		if (Stimulus.WasSuccessfullySensed())
		{
			DEBUG_LOG(TEXT("Player detected by Boss: %s"), *Actor->GetName());

			if (!bHealthWidgetActive)
			{
				DetectedPlayer = Player;
				CreateAndAttachHealthWidget();
				PlayBossBGM();
			}
		}
		else
		{
			DEBUG_LOG(TEXT("Player lost by Boss: %s"), *Actor->GetName());

			if (Actor == DetectedPlayer.Get())
			{
				RemoveHealthWidget();
			}
		}
	}
}

void ABossCharacter::CreateAndAttachHealthWidget()
{
	if (bHealthWidgetActive)
	{
		DEBUG_LOG(TEXT("BossHealthWidget already active"));
		return;
	}

	if (!BossHealthWidgetClass)
	{
		DEBUG_LOG(TEXT("BossHealthWidgetClass is not set"));
		return;
	}
	
	BossHealthWidget = CreateWidget<UBossHealthWidget>(GetWorld(), BossHealthWidgetClass);
	if (!BossHealthWidget)
	{
		DEBUG_LOG(TEXT("Failed to create BossHealthWidget"));
		return;
	}

	UBossAttributeSet* BossAttributeSet = GetAttributeSet();
	if (BossAttributeSet)
	{
		BossHealthWidget->SetBossAttributeSet(BossAttributeSet);
	}

	//보스 이름 설정
	BossHealthWidget->SetBossName(EnemyName);

	BossHealthWidget->AddToViewport();

	bHealthWidgetActive = true;
	DEBUG_LOG(TEXT("BossHealthWidget created and attached"));
}

void ABossCharacter::RemoveHealthWidget()
{
	if (!bHealthWidgetActive)
	{
		return;
	}

	if (BossHealthWidget)
	{
		BossHealthWidget->RemoveFromParent();
		BossHealthWidget = nullptr;
		DEBUG_LOG(TEXT("BossHealthWidget removed"));
	}

	StopBossBGM();

	DetectedPlayer.Reset();
	bHealthWidgetActive = false;
}

void ABossCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABossCharacter::RotateToTarget(const AActor* TargetActor, float RotateTime)
{
	if (!TargetActor)
	{
		DEBUG_LOG(TEXT("RotateToTarget: TargetActor is null"));
		return;
	}

	//BaseCharacter의 RotateToPosition 호출
	RotateToPosition(TargetActor->GetActorLocation(), RotateTime);
}

void ABossCharacter::PlayBossBGM()
{
	if (!BossBGM)
	{
		DEBUG_LOG(TEXT("BossBGM is not set"));
		return;
	}

	//이미 재생 중이면 리턴
	if (BGMAudioComponent && BGMAudioComponent->IsPlaying())
	{
		DEBUG_LOG(TEXT("BossBGM is already playing"));
		return;
	}

	BGMAudioComponent = UGameplayStatics::SpawnSound2D(this, BossBGM);
	if (BGMAudioComponent)
	{
		DEBUG_LOG(TEXT("BossBGM started playing"));
	}
}

void ABossCharacter::StopBossBGM()
{
	if (BGMAudioComponent && BGMAudioComponent->IsPlaying())
	{
		BGMAudioComponent->Stop();
		DEBUG_LOG(TEXT("BossBGM stopped"));
	}
}