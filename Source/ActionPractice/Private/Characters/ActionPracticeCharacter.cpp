#include "Public/Characters/ActionPracticeCharacter.h"
#include "Characters/LockOnComponent.h"
#include "Characters/WeaponManagerComponent.h"
#include "Characters/ItemManagerComponent.h"
#include "Public/Games/ActionPracticePlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "GameplayAbilities/Public/Abilities/GameplayAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Input/InputBufferComponent.h"
#include "Blueprint/UserWidget.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "UI/PlayerStatsWidget.h"
#include "UI/EquipmentSlotWidget.h"
#include "Input/InputActionDataAsset.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

// 디버그 로그 활성화/비활성화 (0: 비활성화, 1: 활성화)
#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeCharacter, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeCharacter, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

AActionPracticeCharacter::AActionPracticeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	//Controller Settings
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	//Character Movement Settings
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	//Camera Boom Settings
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	//FollowCamera Settings
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	//Input Buffer Component Settings
	InputBufferComponent = CreateDefaultSubobject<UInputBufferComponent>(TEXT("InputBufferComponent"));

	//LockOn Component Settings
	LockOnComponent = CreateDefaultSubobject<ULockOnComponent>(TEXT("LockOnComponent"));

	//WeaponManager Component Settings
	WeaponManagerComponent = CreateDefaultSubobject<UWeaponManagerComponent>(TEXT("WeaponManagerComponent"));

	//ItemManager Component Settings
	ItemManagerComponent = CreateDefaultSubobject<UItemManagerComponent>(TEXT("ItemManagerComponent"));

	//GAS Settings
	CreateAbilitySystemComponent();
	CreateAttributeSet();
}

void AActionPracticeCharacter::BeginPlay()
{
	Super::BeginPlay();

	//태그 초기화
	StateRecoveringLocalTag = UGameplayTagsSubsystem::GetStateRecoveringLocalTag();
	StateAbilitySprintingTag = UGameplayTagsSubsystem::GetStateAbilitySprintingTag();
	StateAbilityRollingTag = UGameplayTagsSubsystem::GetStateAbilityRollingTag();
	StateAbilityAttackingLocalTag = UGameplayTagsSubsystem::GetStateAbilityAttackingLocalTag();
	AbilityAttackTag = UGameplayTagsSubsystem::GetAbilityAttackTag();
	EventActionAttackInputTag = UGameplayTagsSubsystem::GetEventActionAttackInputTag();
	EventActionCancelAttackTag = UGameplayTagsSubsystem::GetEventActionCancelAttackTag();
	StateCanMoveTag = UGameplayTagsSubsystem::GetStateCanMoveTag();

	if (!StateRecoveringLocalTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateRecoveringTag is not valid"));
	}
	if (!StateAbilitySprintingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilitySprintingTag is not valid"));
	}
	if (!StateAbilityRollingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityRollingTag is not valid"));
	}
	if (!StateAbilityAttackingLocalTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityAttackingLocalTag is not valid"));
	}
	if (!AbilityAttackTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityAttackTag is not valid"));
	}
	if (!EventActionAttackInputTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventActionAttackInputTag is not valid"));
	}
	if (!EventActionCancelAttackTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventActionCancelAttackTag is not valid"));
	}
	
	//InitializeAbilitySystem();

	if (PlayerStatsWidgetClass)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			PlayerStatsWidget = CreateWidget<UPlayerStatsWidget>(PC, PlayerStatsWidgetClass);
			if (PlayerStatsWidget)
			{
				PlayerStatsWidget->AddToViewport();
				
				//AttributeSet 연결
				if (AttributeSet)
				{
					PlayerStatsWidget->SetAttributeSet(GetAttributeSet());
					DEBUG_LOG(TEXT("PlayerStatsWidget created and AttributeSet connected"));
				}
				else
				{
					DEBUG_LOG(TEXT("AttributeSet is nullptr!"));
				}
			}
		}
		else
		{
			DEBUG_LOG(TEXT("PlayerController is nullptr!"));
		}
	}
	else
	{
		DEBUG_LOG(TEXT("PlayerStatsWidgetClass is not set!"));
	}

	if (EquipmentSlotWidgetClass)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			EquipmentSlotWidget = CreateWidget<UEquipmentSlotWidget>(PC, EquipmentSlotWidgetClass);
			if (EquipmentSlotWidget)
			{
				EquipmentSlotWidget->AddToViewport();

				//데이터 소스 연결
				EquipmentSlotWidget->SetDataSources(WeaponManagerComponent, ItemManagerComponent);
				DEBUG_LOG(TEXT("EquipmentSlotWidget created and data sources connected"));
			}
		}
	}
	else
	{
		DEBUG_LOG(TEXT("EquipmentSlotWidgetClass is not set!"));
	}

	TryAutoActivateAttackSequenceAbility();
}

void AActionPracticeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AActionPracticeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 입력 바인딩은 AActionPracticePlayerController::SetupInputComponent 에서 처리
}

#pragma region "Move Functions"
void AActionPracticeCharacter::ExecuteMove(const FVector2D& MovementVector)
{
	//리커버리가 끝나면 어빌리티 중단
	if (MovementVector.Size() > 0.1f)
	{
		CancelActionForMove();
	}

	bool bIsRecovering = AbilitySystemComponent->HasMatchingGameplayTag(StateRecoveringLocalTag);
	bool bCanMoveDuringRecovery = AbilitySystemComponent->HasMatchingGameplayTag(StateCanMoveTag);

	//리커버리 중이고 CanMove가 없으면 이동 차단 (기존 동작)
	if (bIsRecovering && !bCanMoveDuringRecovery)
	{
		return;
	}

	//이동 입력 스케일 결정 (리커버리 + CanMove면 감속)
	float MoveScale = (bIsRecovering && bCanMoveDuringRecovery) ? RecoveryMoveSpeedMultiplier : 1.0f;

	if (Controller != nullptr)
	{
		bool bIgnoreLockOnState = AbilitySystemComponent->HasMatchingGameplayTag(StateAbilitySprintingTag)
			|| AbilitySystemComponent->HasMatchingGameplayTag(StateAbilityRollingTag);

		//락온 상태에서 걸을 때: Strafe 이동 (Sprint, Roll 중에는 제외)
		if (!bIgnoreLockOnState && LockOnComponent->IsLockedOn() && LockOnComponent->GetLockOnTarget())
		{
			//Strafe 이동 설정: CMC가 ControlRotation을 따르도록 설정
			LockOnComponent->SetRotationMode(false, true);

			const FVector TargetLocation = LockOnComponent->GetLockOnTarget()->GetActorLocation();
			const FVector CharacterLocation = GetActorLocation();

			//타겟 방향 계산
			FVector DirectionToTarget = TargetLocation - CharacterLocation;
			DirectionToTarget.Z = 0.0f; //수평 방향만 고려
			DirectionToTarget.Normalize();

			//타겟을 기준으로 한 이동 방향 계산
			const FRotator TargetRotation = DirectionToTarget.Rotation();
			const FVector RightDirection = FRotationMatrix(TargetRotation).GetUnitAxis(EAxis::Y);
			const FVector BackwardDirection = -DirectionToTarget; //타겟 반대 방향

			//Strafe 이동
			AddMovementInput(RightDirection, MovementVector.X * MoveScale);

			//전후 이동
			AddMovementInput(BackwardDirection, -MovementVector.Y * MoveScale);

			//회전은 CMC가 ControlRotation 기반으로 처리 (Controller의 UpdateLockOnCamera에서 설정)
		}

		else //일반적인 회전 이동 (락온 없음 or 락온+달리기)
		{
			//일반 회전 이동 설정
			LockOnComponent->SetRotationMode(true, false);

			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			AddMovementInput(ForwardDirection, MovementVector.Y * MoveScale);
			AddMovementInput(RightDirection, MovementVector.X * MoveScale);
		}
	}
}

FVector2D AActionPracticeCharacter::GetCurrentMovementInput() const
{
	// 로컬 입력은 로컬 컨트롤 폰에서만 유효 (데디케이티드 서버·시뮬레이티드 프록시는 ZeroVector)
	if (!IsLocallyControlled()) return FVector2D::ZeroVector;

	AActionPracticePlayerController* PC = GetController<AActionPracticePlayerController>();
	UInputAction* MoveAction = PC ? PC->GetIA_Move() : nullptr;
	if (PC && MoveAction)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			//특정 액션의 현재 값 조회
			FInputActionValue ActionValue = Subsystem->GetPlayerInput()->GetActionValue(MoveAction);
			return ActionValue.Get<FVector2D>();
		}
	}
	return FVector2D::ZeroVector;
}

bool AActionPracticeCharacter::IsBlockInputPressed() const
{
	// 로컬 입력은 로컬 컨트롤 폰에서만 유효 (데디케이티드 서버·시뮬레이티드 프록시는 false)
	if (!IsLocallyControlled()) return false;

	AActionPracticePlayerController* PC = GetController<AActionPracticePlayerController>();
	UInputAction* BlockAction = PC ? PC->GetIA_Block() : nullptr;
	if (PC && BlockAction)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// IA_Block 액션의 현재 값 조회
			FInputActionValue ActionValue = Subsystem->GetPlayerInput()->GetActionValue(BlockAction);
			return ActionValue.Get<bool>();
		}
	}
	return false;
}

void AActionPracticeCharacter::RotateCharacterToInputDirection(float RotateTime, bool bIgnoreLockOn)
{
	float DesiredYaw = 0.0f;

	//락온 상태면 락온 대상 방향으로
	if (!bIgnoreLockOn && LockOnComponent->IsLockedOn())
	{
		AActor* LockTarget = LockOnComponent->GetLockOnTarget();
		if (!LockTarget) return;
		DEBUG_LOG(TEXT("APCharacter - Rotate Character To Lock On Target"));

		//락온 타겟 방향으로 Yaw 계산
		const FVector DirectionToTarget = (LockTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		DesiredYaw = DirectionToTarget.Rotation().Yaw;
	}
	
	//아니면 입력 방향으로
	else
	{
		if (!CalculateYawFromMovementInput(DesiredYaw)) return;
		DEBUG_LOG(TEXT("APCharacter - Rotate Character To Movement Input Direction"));
	}

	const FRotator TargetRotation(0.0f, DesiredYaw, 0.0f);

	//로컬 회전
	DEBUG_LOG(TEXT("APCharacter - RotateToRotation called. HasAuthority: %s"), HasAuthority() ? TEXT("true") : TEXT("false"));
	RotateToRotation(TargetRotation, RotateTime);
	
	//서버 회전 RPC
	if (!HasAuthority())
	{
		DEBUG_LOG(TEXT("APCharacter - Server_RequestRotateToYaw RPC called"));
		Server_RequestRotateToYaw(DesiredYaw, RotateTime);
	}
}

void AActionPracticeCharacter::Server_RequestRotateToYaw_Implementation(float TargetYaw, float RotateTime)
{
	const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);
	RotateToRotation(TargetRotation, RotateTime);
}

bool AActionPracticeCharacter::CalculateYawFromMovementInput(float& OutYaw) const
{
	const FVector2D MovementInput = GetCurrentMovementInput();
	if (MovementInput.IsZero())
	{
		return false;
	}

	const FRotator CameraRotation = FollowCamera->GetComponentRotation();
	const FRotator CameraYaw(0.0f, CameraRotation.Yaw, 0.0f);

	const FVector InputDirection(MovementInput.Y, MovementInput.X, 0.0f);
	FVector WorldDirection = CameraYaw.RotateVector(InputDirection);
	WorldDirection.Z = 0.0f;

	if (!WorldDirection.Normalize())
	{
		return false;
	}

	OutYaw = WorldDirection.Rotation().Yaw;
	return true;
}

void AActionPracticeCharacter::CancelActionForMove()
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	
	//Attack 어빌리티가 활성화되어 있는지 확인
	bool bHasActiveAttackAbility = AbilitySystemComponent->HasMatchingGameplayTag(StateAbilityAttackingLocalTag);
	if (bHasActiveAttackAbility)
	{
		//State.Recovering.Local 태그가 없으면 어빌리티 캔슬 가능 (ActionRecoveryEnd 이후)
		if (!AbilitySystemComponent->HasMatchingGameplayTag(StateRecoveringLocalTag))
		{
			//공격 취소 이벤트 전송
			FGameplayEventData EventData;
			EventData.EventTag = EventActionCancelAttackTag;
			
			APASC->HandleGameplayEvent_NetPredicted(EventActionCancelAttackTag, &EventData);
			DEBUG_LOG(TEXT("Attack Montage Cancelled by Move Input"));
		}
		else
		{
			DEBUG_LOG(TEXT("Attack is in Recovering state - cannot cancel"));
		}
	}
}
#pragma endregion

#pragma region "Look Functions"
void AActionPracticeCharacter::ExecuteLook(const FVector2D& LookAxisVector)
{
	if (LockOnComponent->IsLockedOn() && LockOnComponent->GetLockOnTarget()) return;

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}
#pragma endregion

#pragma region "Weapon Functions"

TScriptInterface<IHitDetectionInterface> AActionPracticeCharacter::GetHitDetectionInterface() const
{
	if (!WeaponManagerComponent) return nullptr;
	return WeaponManagerComponent->GetHitDetectionInterface();
}

void AActionPracticeCharacter::ResetAttackSequenceAbility()
{
	if (!AbilitySystemComponent) return;

	//현재 활성화된 AttackSequenceAbility 취소
	const FGameplayTag AbilityAttackNormalTag = UGameplayTagsSubsystem::GetAbilityAttackNormalTag();

	for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!Spec.Ability) continue;

		if (Spec.Ability->GetAssetTags().HasTag(AbilityAttackNormalTag))
		{
			if (Spec.IsActive())
			{
				AbilitySystemComponent->CancelAbilityHandle(Spec.Handle);
				DEBUG_LOG(TEXT("ResetAttackSequenceAbility: Cancelled active AttackSequenceAbility"));
			}
			break;
		}
	}

	//재활성화 플래그 초기화
	bAttackSequenceAutoActivated = false;
	AttackSequenceAutoActivateRetryCount = 0;

	//재활성화 시도
	TryAutoActivateAttackSequenceAbility();
}

#pragma endregion

#pragma region "GAS Functions"
FGameplayTagContainer AActionPracticeCharacter::CaptureCurrentStateTags() const
{
	FGameplayTagContainer StateTags;

	if (!APASC)
	{
		return StateTags;
	}

	FGameplayTagContainer CurrentTags;
	APASC->GetOwnedGameplayTags(CurrentTags);

	static const FGameplayTag StateParentTag = FGameplayTag::RequestGameplayTag(FName("State"));
	for (const FGameplayTag& Tag : CurrentTags)
	{
		if (Tag.MatchesTag(StateParentTag))
		{
			StateTags.AddTag(Tag);
		}
	}

	return StateTags;
}

void AActionPracticeCharacter::InitializeAbilitySystem()
{
	Super::InitializeAbilitySystem();
}

void AActionPracticeCharacter::CreateAbilitySystemComponent()
{
	AbilitySystemComponent = CreateDefaultSubobject<UActionPracticeAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	APASC = Cast<UActionPracticeAbilitySystemComponent>(AbilitySystemComponent);
}

void AActionPracticeCharacter::CreateAttributeSet()
{
	AttributeSet = CreateDefaultSubobject<UActionPracticeAttributeSet>(TEXT("AttributeSet"));
}

void AActionPracticeCharacter::GASInputPressed(const UInputAction* InputAction)
{
	if (!APASC || !InputAction) return;
	
	TArray<FGameplayAbilitySpec*> TryActivateSpecs = FindAbilitySpecsWithInputAction(InputAction);
	if (TryActivateSpecs.IsEmpty()) return;

	//다른 어빌리티가 수행중이고 입력 저장 가능할 때는 버퍼링
	if (InputBufferComponent->bBufferWindowOpened)
	{
		DEBUG_LOG(TEXT("Character: Buffer"));
		InputBufferComponent->BufferInput(InputAction, false);
	}
	else
	{
		for (auto& Spec : TryActivateSpecs)
		{
			//해당 어빌리티가 이미 실행중이면 이벤트 전달
			if (Spec->IsActive())
			{
				DEBUG_LOG(TEXT("GASInputPressed: Ability already active, calling Input Event - %s"), *GetNameSafe(Spec->Ability));
				Spec->InputPressed = true;

				FGameplayEventData EventData;
				EventData.InstigatorTags.AddTag(InputActionData->FindTagByInputAction(InputAction)); //타깃 어빌리티만 활성화

				//현재 상태 태그 추가
				EventData.InstigatorTags.AppendTags(CaptureCurrentStateTags());

				EventData.EventMagnitude = 0.0f; //Pressed
				EventData.EventTag = EventActionAttackInputTag;

				APASC->HandleGameplayEvent_NetPredicted(EventActionAttackInputTag, &EventData);

				//레거시: 실행 중인 어빌리티에 Pressed 전달은 Attack밖에 없기 때문에 기존 Pressed 비활성화, IA를 전달하는 이벤트 송신으로 변경
				//AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
			}
			//해당 어빌리티가 비활성화 상태면
			else
			{
				Spec->InputPressed = true;

				//InputActionTag를 EventData에 담아 전달
				FGameplayEventData EventData;
				EventData.InstigatorTags.AddTag(InputActionData->FindTagByInputAction(InputAction));

				bool bSuccess = APASC->TryActivateAbilityWithEventData(Spec->Handle, &EventData);
				DEBUG_LOG(TEXT("GASInputPressed: TryActivateAbility %s - %s"), bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"), *GetNameSafe(Spec->Ability));
			}
		}
	}
}

void AActionPracticeCharacter::GASInputReleased(const UInputAction* InputAction)
{
	if (!APASC || !InputAction) return;
	
	TArray<FGameplayAbilitySpec*> TryActivateSpecs = FindAbilitySpecsWithInputAction(InputAction);
	if (TryActivateSpecs.IsEmpty()) return;

	//버퍼 ON/OFF와 무관하게, 현재 활성화된 어빌리티가 있으면 릴리즈를 먼저 전달
	for (auto& Spec : TryActivateSpecs)
	{
		if (!Spec->IsActive())
			continue;

		Spec->InputPressed = false;
		APASC->AbilitySpecInputReleased(*Spec);
	}

	//릴리즈 버퍼링
	if (InputBufferComponent && InputBufferComponent->bBufferWindowOpened)
	{
		DEBUG_LOG(TEXT("Character UnBuffer"));
		InputBufferComponent->BufferInput(InputAction, true);
	}
}

TArray<FGameplayAbilitySpec*> AActionPracticeCharacter::FindAbilitySpecsWithInputAction(const UInputAction* InputAction)
{
	TArray<FGameplayAbilitySpec*> SameAssetSpecs;
	if (!AbilitySystemComponent) return SameAssetSpecs;
	
	const FInputActionAbilityRule* Rule = InputActionData->FindRuleByAction(InputAction);
	if (!Rule)
	{
		DEBUG_LOG(TEXT("FindAbilitySpecsWithInputAction: No Rule"));
		return SameAssetSpecs;
	}
	
	const FGameplayTagContainer* InputAssetTags = &Rule->AbilityAssetTags;
	if (!InputAssetTags)
	{
		DEBUG_LOG(TEXT("FindAbilitySpecsWithInputAction: No InputAssetTags"));
		return SameAssetSpecs;
	}
    
	for (auto& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability)
		{  
			if (Spec.Ability->GetAssetTags().HasAll(*InputAssetTags) || Spec.GetDynamicSpecSourceTags().HasAll(*InputAssetTags))
			{
				SameAssetSpecs.Add(&Spec);
			}
		}
	}

	return SameAssetSpecs;
}
#pragma endregion

#pragma region "Replication Functions"
void AActionPracticeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

bool AActionPracticeCharacter::IsAttackSequenceAutoActivateReady() const
{
	if (!IsLocallyControlled()) return false;
	if (!AbilitySystemComponent) return false;

	AWeapon* RW = GetRightWeapon();
	if (!RW) return false;
	if (!RW->GetWeaponData()) return false;
	if (!RW->GetHitDetectionComponent()) return false;

	return true;
}

void AActionPracticeCharacter::TryAutoActivateAttackSequenceAbility()
{
	//이미 성공했으면 중복 시도 방지
	if (bAttackSequenceAutoActivated)
	{
		return;
	}

	//준비 안 됐으면 약간 딜레이 후 재시도 (복제/OnRep 타이밍 흡수)
	if (!IsAttackSequenceAutoActivateReady())
	{
		//무한 루프 방지 (필요하면 값 조정)
		constexpr int32 MaxRetry = 120; //0.05s * 120 = 6초
		if (AttackSequenceAutoActivateRetryCount++ >= MaxRetry)
		{
			DEBUG_LOG(TEXT("AttackSequence auto-activate failed: retry limit reached. RightWeapon=%s"),
				*GetNameSafe(GetRightWeapon()));
			return;
		}

		if (GetWorld())
		{
			GetWorldTimerManager().SetTimer(
				AttackSequenceAutoActivateTimer,
				this,
				&AActionPracticeCharacter::TryAutoActivateAttackSequenceAbility,
				0.05f,
				false
			);
		}
		return;
	}

	//활성화 시도
	GetWorldTimerManager().ClearTimer(AttackSequenceAutoActivateTimer);

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	//AssetTags로 AttackSequenceAbility 식별 후 TryActivate
	const FGameplayTag AbilityAttackNormalTag = UGameplayTagsSubsystem::GetAbilityAttackNormalTag();
	const FGameplayTag AbilityAttackChargeTag = UGameplayTagsSubsystem::GetAbilityAttackChargeTag();

	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		const FGameplayTagContainer AssetTags = Spec.Ability->GetAssetTags();
		if (!AssetTags.HasTag(AbilityAttackNormalTag) && !AssetTags.HasTag(AbilityAttackChargeTag))
		{
			continue;
		}

		//이미 활성 상태면 성공 처리
		if (Spec.IsActive())
		{
			bAttackSequenceAutoActivated = true;
			DEBUG_LOG(TEXT("AttackSequence already active - auto-activate complete."));
			return;
		}

		const bool bSuccess = ASC->TryActivateAbility(Spec.Handle, true);
		DEBUG_LOG(TEXT("AttackSequence auto-activate: %s"), bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

		if (bSuccess)
		{
			bAttackSequenceAutoActivated = true;
		}
		return;
	}

	DEBUG_LOG(TEXT("AttackSequence auto-activate: no matching ability spec found."));
}
#pragma endregion
