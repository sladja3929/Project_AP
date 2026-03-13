#include "Characters/BossCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/AbilitySystemComponent/BossAbilitySystemComponent.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "GAS/AttributeSet/BossAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
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
#include "Animation/AnimInstance.h"
#include "BrainComponent.h"
#include "GameplayEffect.h"

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

	//AI 관련 설정은 서버에서만 (싱글플레이어에서는 항상 true)
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

	//초기 스폰 상태 캐시 (Super::BeginPlay 이후, ASC 초기화 완료 후 시점)
	CacheInitialEnemyState();
}

void ABossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//보스는 복제할 추가 변수가 거의 없음
	// DetectedPlayer는 서버 전용, bHealthWidgetActive는 로컬 UI 상태
}

void ABossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveHealthWidget();

	Super::EndPlay(EndPlayReason);
}

#pragma region "Enemy Reset"

void ABossCharacter::CacheInitialEnemyState()
{
	InitialTransform = GetActorTransform();
	DEBUG_LOG(TEXT("CacheInitialEnemyState: Transform cached at %s"), *InitialTransform.GetLocation().ToString());
}

void ABossCharacter::ResetEnemy()
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();

	//진행 중인 어빌리티 전부 취소
	if (ASC)
	{
		ASC->CancelAllAbilities();
	}

	//사망 가드 리셋 (다시 죽음 판정 가능하도록)
	if (UBaseAbilitySystemComponent* BaseASC = Cast<UBaseAbilitySystemComponent>(ASC))
	{
		BaseASC->ResetDeathHandled();
	}

	//사망 관련 태그 제거
	if (ASC)
	{
		const FGameplayTag StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();
		if (StateDeadTag.IsValid())
		{
			while (ASC->HasMatchingGameplayTag(StateDeadTag))
			{
				ASC->RemoveLooseGameplayTag(StateDeadTag);
				ASC->RemoveMinimalReplicationGameplayTag(StateDeadTag);
			}
		}
	}

	//애니메이션 일시정지 해제 및 몽타주 중단
	if (USkeletalMeshComponent* InMesh = GetMesh())
	{
		InMesh->bPauseAnims = false;
		if (UAnimInstance* AnimInst = InMesh->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.0f);
		}
	}

	//이동 복구
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->MovementMode == MOVE_None)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		MoveComp->StopMovementImmediately();
	}

	//초기 위치로 텔레포트
	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);

	//HP/스탯 GE 기반 회복
	ApplyEnemyResetEffect();

	//전투 캐시 초기화
	DetectedPlayer.Reset();
	if (bHealthWidgetActive)
	{
		bHealthWidgetActive = false;
		Multicast_OnBossDisengage();
	}

	//AI 재시작
	RestartEnemyAI();

	DEBUG_LOG(TEXT("ResetEnemy: Complete"));
}

void ABossCharacter::ApplyEnemyResetEffect()
{
	if (!EnemyResetRecoveryEffect)
	{
		DEBUG_LOG(TEXT("ApplyEnemyResetEffect: EnemyResetRecoveryEffect not set"));
		return;
	}

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC) return;

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EnemyResetRecoveryEffect, 1.0f, ASC->MakeEffectContext());
	if (!Spec.IsValid()) return;

	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	DEBUG_LOG(TEXT("ApplyEnemyResetEffect: GE applied"));
}

void ABossCharacter::RestartEnemyAI()
{
	AEnemyAIController* AIController = GetEnemyAIController();
	if (!AIController) return;

	//타깃 캐시 초기화
	AIController->CurrentTarget.Reset();

	//StateTree 재시작
	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		Brain->RestartLogic();
		DEBUG_LOG(TEXT("RestartEnemyAI: StateTree restarted"));
	}
}

#pragma endregion

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
				bHealthWidgetActive = true;

				//서버에서 모든 클라이언트에 조우 연출 전파
				Multicast_OnBossEncounter();
			}
		}
		else
		{
			DEBUG_LOG(TEXT("Player lost by Boss: %s"), *Actor->GetName());

			if (Actor == DetectedPlayer.Get())
			{
				bHealthWidgetActive = false;
				DetectedPlayer.Reset();

				//서버에서 모든 클라이언트에 이탈 연출 전파
				Multicast_OnBossDisengage();
			}
		}
	}
}

void ABossCharacter::CreateAndAttachHealthWidget()
{
	//이미 위젯이 있으면 리턴
	if (BossHealthWidget)
	{
		DEBUG_LOG(TEXT("BossHealthWidget already exists"));
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

	DEBUG_LOG(TEXT("BossHealthWidget created and attached"));
}

void ABossCharacter::RemoveHealthWidget()
{
	if (!BossHealthWidget)
	{
		return;
	}

	BossHealthWidget->RemoveFromParent();
	BossHealthWidget = nullptr;
	DEBUG_LOG(TEXT("BossHealthWidget removed"));
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

void ABossCharacter::Multicast_OnBossEncounter_Implementation()
{
	DEBUG_LOG(TEXT("Multicast_OnBossEncounter called"));

	CreateAndAttachHealthWidget();
	PlayBossBGM();
}

void ABossCharacter::Multicast_OnBossDisengage_Implementation()
{
	DEBUG_LOG(TEXT("Multicast_OnBossDisengage called"));

	RemoveHealthWidget();
	StopBossBGM();
}