#include "Characters/EnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "AI/EnemyAIController.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Characters/HitDetection/EnemyAttackComponent.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "BrainComponent.h"
#include "GameplayEffect.h"
#include "Components/WidgetComponent.h"
#include "UI/EnemyHealthBarWidget.h"
#include "TimerManager.h"
#include "Characters/LockOnComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyCharacter, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyCharacter, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

AEnemyCharacter::AEnemyCharacter()
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
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	//GetCharacterMovement()->bUseAcc
	//Actor Tag
	Tags.Add(FName("Enemy"));

	//Collision Settings
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetMesh()->SetCollisionProfileName(TEXT("HitDetectionPhysics"));

	CreateAbilitySystemComponent();
	CreateAttributeSet();

	//EnemyAttackComponent 생성
	EnemyAttackComponent = CreateDefaultSubobject<UEnemyAttackComponent>(TEXT("EnemyAttackComponent"));

	//머리 위 HP바 WidgetComponent 생성
	EnemyHealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHealthBarWidget"));
	EnemyHealthBarWidgetComponent->SetupAttachment(GetMesh());
	EnemyHealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	EnemyHealthBarWidgetComponent->SetDrawAtDesiredSize(true);
	EnemyHealthBarWidgetComponent->SetVisibility(false);
	EnemyHealthBarWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
}

TScriptInterface<IHitDetectionInterface> AEnemyCharacter::GetHitDetectionInterface() const
{
	return EnemyAttackComponent;
}

void AEnemyCharacter::CreateAbilitySystemComponent()
{
	AbilitySystemComponent = CreateDefaultSubobject<UEnemyAbilitySystemComponent>(TEXT("EnemyAbilitySystemComponent"));
}

void AEnemyCharacter::CreateAttributeSet()
{
	AttributeSet = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("EnemyAttributeSet"));
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	DEBUG_LOG(TEXT("EnemyCharacter::BeginPlay: This=%p, ASC=%p, AttributeSet=%p"),
		this,
		AbilitySystemComponent.Get(),
		AttributeSet.Get());

	if (EnemyData)
	{
		DEBUG_LOG(TEXT("EnemyCharacter::BeginPlay: EnemyData=%s"), *GetNameSafe(EnemyData));
	}
	else
	{
		DEBUG_LOG(TEXT("EnemyCharacter::BeginPlay: EnemyData is nullptr"));
	}

	DEBUG_LOG(TEXT("EnemyCharacter::BeginPlay: StartAbilities count=%d"), StartAbilities.Num());
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
		AEnemyAIController* EnemyController = GetEnemyAIController();
		if (EnemyController)
		{
			UAIPerceptionComponent* PerceptionComponent = EnemyController->GetPerceptionComponent();
			if (PerceptionComponent)
			{
				PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyCharacter::OnPlayerDetected);
				DEBUG_LOG(TEXT("Perception delegate bound to EnemyCharacter"));
			}
		}
	}

	//초기 스폰 상태 캐시 (Super::BeginPlay 이후, ASC 초기화 완료 후 시점)
	CacheInitialEnemyState();

	//HP바 위젯 초기화 (로컬에서만 의미 있음)
	if (EnemyHealthBarWidgetClass && EnemyHealthBarWidgetComponent)
	{
		EnemyHealthBarWidgetComponent->SetWidgetClass(EnemyHealthBarWidgetClass);
		EnemyHealthBarWidgetComponent->InitWidget();

		if (UEnemyHealthBarWidget* HealthBarWidget = Cast<UEnemyHealthBarWidget>(EnemyHealthBarWidgetComponent->GetWidget()))
		{
			HealthBarWidget->SetAttributeSet(GetAttributeSet());
		}
	}
}

void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HealthBarVisibilityTimer);
	Super::EndPlay(EndPlayReason);
}

#pragma region "Enemy Health Bar"

void AEnemyCharacter::ShowEnemyHealthBar()
{
	if (!EnemyHealthBarWidgetComponent) return;

	EnemyHealthBarWidgetComponent->SetVisibility(true);

	//타이머 리셋 — 이미 락온 중이면 타이머 불필요
	if (!bLockedOnByPlayer)
	{
		GetWorldTimerManager().SetTimer(
			HealthBarVisibilityTimer,
			this,
			&AEnemyCharacter::OnHealthBarTimerExpired,
			HealthBarVisibilityDuration,
			false
		);
	}
}

void AEnemyCharacter::HideEnemyHealthBar()
{
	if (!EnemyHealthBarWidgetComponent) return;

	EnemyHealthBarWidgetComponent->SetVisibility(false);
	GetWorldTimerManager().ClearTimer(HealthBarVisibilityTimer);
}

void AEnemyCharacter::SetLockedOnByPlayer(bool bLocked)
{
	bLockedOnByPlayer = bLocked;

	if (bLocked)
	{
		//락온 시 HP바 표시, 타이머 정지
		if (EnemyHealthBarWidgetComponent)
		{
			EnemyHealthBarWidgetComponent->SetVisibility(true);
		}
		GetWorldTimerManager().ClearTimer(HealthBarVisibilityTimer);
	}
	else
	{
		//락온 해제 시 타이머 시작 (바로 숨기지 않고 잠시 후 숨김)
		GetWorldTimerManager().SetTimer(
			HealthBarVisibilityTimer,
			this,
			&AEnemyCharacter::OnHealthBarTimerExpired,
			HealthBarVisibilityDuration,
			false
		);
	}
}

void AEnemyCharacter::Multicast_ReleaseLockOn_Implementation()
{
	//로컬 플레이어의 LockOnComponent가 이 적을 타깃으로 하고 있으면 해제
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn) return;

	ULockOnComponent* LockOnComp = PlayerPawn->FindComponentByClass<ULockOnComponent>();
	if (!LockOnComp) return;

	if (LockOnComp->GetLockOnTarget() == this)
	{
		LockOnComp->SetLockedOnTarget(nullptr);
	}
}

void AEnemyCharacter::OnHealthBarTimerExpired()
{
	if (!bLockedOnByPlayer)
	{
		HideEnemyHealthBar();
	}
}

#pragma endregion

#pragma region "Enemy Reset"

void AEnemyCharacter::CacheInitialEnemyState()
{
	InitialTransform = GetActorTransform();
	DEBUG_LOG(TEXT("CacheInitialEnemyState: Transform cached at %s"), *InitialTransform.GetLocation().ToString());
}

void AEnemyCharacter::ResetEnemy()
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

	//AI 재시작
	RestartEnemyAI();

	DEBUG_LOG(TEXT("ResetEnemy: Complete"));
}

void AEnemyCharacter::ApplyEnemyResetEffect()
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

void AEnemyCharacter::RestartEnemyAI()
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

void AEnemyCharacter::OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	//시각 감지 케이스 — 기본 구현은 DetectedPlayer 캐시만 수행
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(Actor);
		if (!Player) return;

		if (Stimulus.WasSuccessfullySensed())
		{
			DetectedPlayer = Player;
			DEBUG_LOG(TEXT("Player detected by Enemy: %s"), *Actor->GetName());
		}
		else
		{
			if (Actor == DetectedPlayer.Get())
			{
				DetectedPlayer.Reset();
				DEBUG_LOG(TEXT("Player lost by Enemy: %s"), *Actor->GetName());
			}
		}
	}
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemyCharacter::RotateToTarget(const AActor* TargetActor, float RotateTime)
{
	if (!TargetActor)
	{
		DEBUG_LOG(TEXT("RotateToTarget: TargetActor is null"));
		return;
	}

	//BaseCharacter의 RotateToPosition 호출
	RotateToPosition(TargetActor->GetActorLocation(), RotateTime);
}
