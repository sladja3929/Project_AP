#include "Characters/EnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "AI/EnemyAIController.h"
#include "Characters/ActionPracticeCharacter.h"
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

	//가속 기반 경로 이동. 비활성 시 MoveTo 중단(StateTree 이동 상태 전환 등)마다 속도가 0으로 리셋되어 로코모션이 한 프레임 Idle로 튐
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;

	//Actor Tag
	Tags.Add(FName("Enemy"));

	//Collision Settings
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetMesh()->SetCollisionProfileName(TEXT("HitDetectionPhysics"));
	//USkeletalMeshComponent은 기본적으로 오버랩 이벤트를 생성하지 않음(생성자에서 false)
	//CapsuleOverlapComponent(순수 오버랩 방식)가 이 메시와 OnComponentBeginOverlap을 받으려면 명시적으로 켜야 함
	//AttackTrace(SweepByChannel)는 이 플래그와 무관하므로 영향 없음
	GetMesh()->SetGenerateOverlapEvents(true);

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

void AEnemyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//시체 충돌 복구용 원래 캡슐 응답 캐시 — 클라이언트 초기 복제(OnRep_IsCorpse)가 BeginPlay보다 먼저 올 수 있어 복제 전 시점에 캐시
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		DefaultCapsulePawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	//서버 AI는 PossessedBy에서 이미 초기화됨 (중복 호출은 무시). 클라이언트·빙의되지 않은 적을 위한 안전망
	InitializeAbilitySystem();

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

	//EnemyData의 모든 몽타주 프리로드
	if (EnemyData)
	{
		EnemyData->PreloadAllMontages();
	}

	//초기 스폰 상태 캐시 (Super::BeginPlay 이후, ASC 초기화 완료 후 시점)
	CacheInitialEnemyState();

	//HP바 위젯 초기화 (로컬에서만 의미 있음)
	if (EnemyHealthBarWidgetComponent)
	{
		if (UEnemyHealthBarWidget* HealthBarWidget = Cast<UEnemyHealthBarWidget>(EnemyHealthBarWidgetComponent->GetWidget()))
		{
			HealthBarWidget->SetAttributeSet(GetAttributeSet());
		}
	}
}

void AEnemyCharacter::PossessedBy(AController* NewController)
{
	//Controller·Owner 설정 후 초기화
	Super::PossessedBy(NewController);

	//배치된 적은 BeginPlay보다 빙의가 먼저 일어나고, 컨트롤러 OnPossess가 이 함수 직후 StateTree를 시작함
	//BeginPlay에서 처음 InitAbilityActorInfo를 부르면 그 사이 StateTree가 재생한 몽타주 정보(LocalAnimMontageInfo)가 초기화되어
	//ASC는 재생 중이 아니라고 판단하고 몽타주는 정지되지 않은 채 남음 (첫 Rest 몽타주가 배회 중에도 계속 재생되던 원인)
	InitializeAbilitySystem();

	//전투 상태 델리게이트 바인딩 (런타임 스폰·재빙의 포함, 중복 방지)
	if (AEnemyAIController* EnemyController = Cast<AEnemyAIController>(NewController))
	{
		EnemyController->OnCombatStateChanged.RemoveAll(this);
		EnemyController->OnCombatStateChanged.AddUObject(this, &AEnemyCharacter::OnCombatStateChanged);
		DEBUG_LOG(TEXT("Combat state delegate bound to EnemyCharacter"));
	}
}

void AEnemyCharacter::UnPossessed()
{
	if (AEnemyAIController* EnemyController = GetEnemyAIController())
	{
		EnemyController->OnCombatStateChanged.RemoveAll(this);
	}

	CombatTarget = nullptr;

	Super::UnPossessed();
}

void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnemyCharacter, CombatTarget);
	DOREPLIFETIME(AEnemyCharacter, bIsCorpse);
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HealthBarVisibilityTimer);

	if (AEnemyAIController* EnemyController = GetEnemyAIController())
	{
		EnemyController->OnCombatStateChanged.RemoveAll(this);
	}

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

void AEnemyCharacter::Multicast_FreezeAnimPose_Implementation()
{
	if (USkeletalMeshComponent* InMesh = GetMesh())
	{
		InMesh->bPauseAnims = true;
	}
}

void AEnemyCharacter::SetCorpseCollision(bool bCorpse)
{
	if (!HasAuthority()) return;

	bIsCorpse = bCorpse;
	ApplyCorpseCollision();
}

void AEnemyCharacter::OnRep_IsCorpse()
{
	ApplyCorpseCollision();
}

void AEnemyCharacter::ApplyCorpseCollision()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule) return;

	//Pawn 채널만 해제 — WorldStatic 등 지형 응답은 유지
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, bIsCorpse ? ECR_Ignore : DefaultCapsulePawnResponse.GetValue());
	DEBUG_LOG(TEXT("ApplyCorpseCollision: %s, bIsCorpse=%d"), *GetName(), bIsCorpse);
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
		//해제만 하지 않고 자동 재지정까지 컴포넌트에 위임한다 (내부에서 로컬 컨트롤 여부를 검사)
		LockOnComp->HandleTargetLost(this);
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
	SpawnLocation = InitialTransform.GetLocation();
	DEBUG_LOG(TEXT("CacheInitialEnemyState: Transform cached at %s"), *SpawnLocation.ToString());
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

	//시체 충돌 복구 (텔레포트 후 — 사망 위치에서 플레이어와 겹친 채 복구되지 않도록)
	SetCorpseCollision(false);

	//DA 기반 어트리뷰트 재초기화
	ApplyInitialAttributes();

	//AI 재시작
	RestartEnemyAI();

	//사망 경로는 전투 해제 알림이 없으므로 리셋 시 연출용 전투 대상도 직접 정리
	CombatTarget = nullptr;

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

	//전투·인지 상태 초기화
	AIController->ResetAwareness();

	//StateTree 재시작
	if (UBrainComponent* Brain = AIController->GetBrainComponent())
	{
		Brain->RestartLogic();
		DEBUG_LOG(TEXT("RestartEnemyAI: StateTree restarted"));
	}
}

#pragma endregion

void AEnemyCharacter::OnCombatStateChanged(bool bInCombat, AActor* Target)
{
	//연출용 전투 대상 복제 (보스는 조우 연출을 추가로 override)
	CombatTarget = bInCombat ? Target : nullptr;

	DEBUG_LOG(TEXT("OnCombatStateChanged: %s, Target=%s"), bInCombat ? TEXT("Enter") : TEXT("Exit"), *GetNameSafe(Target));
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
