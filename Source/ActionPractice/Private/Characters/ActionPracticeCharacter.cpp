#include "Public/Characters/ActionPracticeCharacter.h"
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

	EquipWeapon(RightWeaponClass, false, false);
	EquipWeapon(LeftWeaponClass, true, false);

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

	if (Controller != nullptr && !bIsRecovering)
	{
		bool bIgnoreLockOnState = AbilitySystemComponent->HasMatchingGameplayTag(StateAbilitySprintingTag)
			|| AbilitySystemComponent->HasMatchingGameplayTag(StateAbilityRollingTag);

		//락온 상태에서 걸을 때: Strafe 이동 (Sprint, Roll 중에는 제외)
		if (!bIgnoreLockOnState && bIsLockOn && LockedOnTarget)
		{
			//Strafe 이동 설정: CMC가 ControlRotation을 따르도록 설정
			SetRotationMode(false, true);

			const FVector TargetLocation = LockedOnTarget->GetActorLocation();
			const FVector CharacterLocation = GetActorLocation();

			//타겟 방향 계산
			FVector DirectionToTarget = TargetLocation - CharacterLocation;
			DirectionToTarget.Z = 0.0f; //수평 방향만 고려
			DirectionToTarget.Normalize();

			//타겟을 기준으로 한 이동 방향 계산
			const FRotator TargetRotation = DirectionToTarget.Rotation();
			const FVector RightDirection = FRotationMatrix(TargetRotation).GetUnitAxis(EAxis::Y);
			const FVector BackwardDirection = -DirectionToTarget; // 타겟 반대 방향

			//Strafe 이동
			AddMovementInput(RightDirection, MovementVector.X);

			//전후 이동
			AddMovementInput(BackwardDirection, -MovementVector.Y);

			//회전은 CMC가 ControlRotation 기반으로 처리 (Controller의 UpdateLockOnCamera에서 설정)
		}

		else //일반적인 회전 이동 (락온 없음 or 락온+달리기)
		{
			//일반 회전 이동 설정
			SetRotationMode(true, false);

			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			AddMovementInput(ForwardDirection, MovementVector.Y);
			AddMovementInput(RightDirection, MovementVector.X);
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
	if (!bIgnoreLockOn && IsLockedOn())
	{
		AActor* LockTarget = GetLockOnTarget();
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

void AActionPracticeCharacter::ServerSetLockOnState_Implementation(bool bNewLockOn, AActor* NewTarget)
{
	bIsLockOn = bNewLockOn;
	LockedOnTarget = NewTarget;

	DEBUG_LOG(TEXT("Server: Lock-On State Updated - bIsLockOn: %s, Target: %s"),
		bNewLockOn ? TEXT("true") : TEXT("false"),
		NewTarget ? *NewTarget->GetName() : TEXT("None"));
}

void AActionPracticeCharacter::ServerSetRotationMode_Implementation(bool bOrientToMovement, bool bUseControllerDesired)
{
	GetCharacterMovement()->bOrientRotationToMovement = bOrientToMovement;
	GetCharacterMovement()->bUseControllerDesiredRotation = bUseControllerDesired;

	DEBUG_LOG(TEXT("Server: RotationMode Updated - OrientToMovement: %s, UseControllerDesired: %s"),
		bOrientToMovement ? TEXT("true") : TEXT("false"),
		bUseControllerDesired ? TEXT("true") : TEXT("false"));
}

void AActionPracticeCharacter::SetRotationMode(bool bOrientToMovement, bool bUseControllerDesired)
{
	//로컬 적용
	GetCharacterMovement()->bOrientRotationToMovement = bOrientToMovement;
	GetCharacterMovement()->bUseControllerDesiredRotation = bUseControllerDesired;

	//값이 바뀌었을 때만 서버 RPC
	if (bCachedOrientToMovement != bOrientToMovement || bCachedUseControllerDesired != bUseControllerDesired)
	{
		bCachedOrientToMovement = bOrientToMovement;
		bCachedUseControllerDesired = bUseControllerDesired;

		if (!HasAuthority())
		{
			ServerSetRotationMode(bOrientToMovement, bUseControllerDesired);
		}
	}
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
	if (bIsLockOn && LockedOnTarget) return;

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AActionPracticeCharacter::SetLockedOnTarget(AActor* NewTarget)
{
	bIsLockOn = (NewTarget != nullptr);
	LockedOnTarget = NewTarget;

	if (bIsLockOn)
	{
		//락온 회전: CMC가 ControlRotation을 따르도록 설정
		SetRotationMode(false, true);
		DEBUG_LOG(TEXT("Lock-On Target: %s"), *NewTarget->GetName());
	}
	else
	{
		//일반 이동 회전으로 복원
		SetRotationMode(true, false);
		DEBUG_LOG(TEXT("Lock-On Released"));
	}

	//서버에 락온 상태 동기화
	if (!HasAuthority())
	{
		ServerSetLockOnState(bIsLockOn, LockedOnTarget);
	}
}
#pragma endregion

#pragma region "Weapon Functions"

TScriptInterface<IHitDetectionInterface> AActionPracticeCharacter::GetHitDetectionInterface() const
{
	if (!RightWeapon) return nullptr;
	return RightWeapon->GetHitDetectionComponent();
}

void AActionPracticeCharacter::WeaponSwitch()
{
}

void AActionPracticeCharacter::EquipWeapon(TSubclassOf<AWeapon> NewWeaponClass, bool bIsLeftHand, bool bIsTwoHanded)
{
	//서버에서만 실행 (싱글플레이어에서는 HasAuthority()가 항상 true)
	if (!HasAuthority())
	{
		return;
	}

	if (!NewWeaponClass) return;

	if(bIsTwoHanded) UnequipWeapon(!bIsLeftHand);
	UnequipWeapon(bIsLeftHand);

	//새 무기 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();

	AWeapon* NewWeapon = GetWorld()->SpawnActor<AWeapon>(NewWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    EWeaponEnums type = NewWeapon->GetWeaponType();

	if (NewWeapon && type != EWeaponEnums::None)
	{
		FString SocketString = bIsLeftHand ? "hand_l" : "hand_r";

		switch (type)
		{
		case EWeaponEnums::StraightSword:
			SocketString += "_sword";
			break;

		case EWeaponEnums::GreatSword:
			SocketString += "_greatsword";
			break;

		case EWeaponEnums::Shield:
			SocketString += "_shield";
			break;
		}

		FName SocketName = FName(*SocketString);
		DEBUG_LOG(TEXT("Equiped Weapon: %s"), *SocketString);
		NewWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

		if(bIsTwoHanded)
		{
			LeftWeapon = NewWeapon;
			RightWeapon = NewWeapon;
		}
		else if (bIsLeftHand)
		{
			LeftWeapon = NewWeapon;
		}

		else
		{
			RightWeapon = NewWeapon;
		}
	}
}

void AActionPracticeCharacter::UnequipWeapon(bool bIsLeftHand)
{
	TObjectPtr<AWeapon>& WeaponToRemove = bIsLeftHand ? LeftWeapon : RightWeapon;

	if (WeaponToRemove)
	{
		WeaponToRemove->Destroy();
		WeaponToRemove = nullptr;
	}
}

TSubclassOf<AWeapon> AActionPracticeCharacter::LoadWeaponClassByName(const FString& WeaponName)
{
	FString BlueprintPath = FString::Printf(TEXT("%s%s.%s_C"), 
										   *WeaponBlueprintBasePath, 
										   *WeaponName, 
										   *WeaponName);
	
	UClass* LoadedClass = LoadClass<AWeapon>(nullptr, *BlueprintPath);
	
	if (LoadedClass && LoadedClass->IsChildOf(AWeapon::StaticClass()))
	{
		return TSubclassOf<AWeapon>(LoadedClass);
	}

	DEBUG_LOG(TEXT("Failed to load weapon class from path: %s"), *BlueprintPath);
	return nullptr;
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
				bool bSuccess = APASC->TryActivateAbility(Spec->Handle);
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

	DOREPLIFETIME(AActionPracticeCharacter, bIsLockOn);
	DOREPLIFETIME(AActionPracticeCharacter, LockedOnTarget);
	DOREPLIFETIME(AActionPracticeCharacter, LeftWeapon);
	DOREPLIFETIME(AActionPracticeCharacter, RightWeapon);
}

void AActionPracticeCharacter::OnRep_LeftWeapon()
{
	if (LeftWeapon && GetMesh())
	{
		//무기 타입에 따라 소켓 이름 결정
		EWeaponEnums type = LeftWeapon->GetWeaponType();
		if (type != EWeaponEnums::None)
		{
			FString SocketString = "hand_l";

			switch (type)
			{
			case EWeaponEnums::StraightSword:
				SocketString += "_sword";
				break;

			case EWeaponEnums::GreatSword:
				SocketString += "_greatsword";
				break;

			case EWeaponEnums::Shield:
				SocketString += "_shield";
				break;
			}

			FName SocketName = FName(*SocketString);
			LeftWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			DEBUG_LOG(TEXT("OnRep_LeftWeapon: Attached to %s"), *SocketString);
		}
	}

	TryAutoActivateAttackSequenceAbility();
}

void AActionPracticeCharacter::OnRep_RightWeapon()
{
	if (RightWeapon && GetMesh())
	{
		//무기 타입에 따라 소켓 이름 결정
		EWeaponEnums type = RightWeapon->GetWeaponType();
		if (type != EWeaponEnums::None)
		{
			FString SocketString = "hand_r";

			switch (type)
			{
			case EWeaponEnums::StraightSword:
				SocketString += "_sword";
				break;

			case EWeaponEnums::GreatSword:
				SocketString += "_greatsword";
				break;

			case EWeaponEnums::Shield:
				SocketString += "_shield";
				break;
			}

			FName SocketName = FName(*SocketString);
			RightWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
			DEBUG_LOG(TEXT("OnRep_RightWeapon: Attached to %s"), *SocketString);
		}
	}

	TryAutoActivateAttackSequenceAbility();
}

bool AActionPracticeCharacter::IsAttackSequenceAutoActivateReady() const
{
	// 로컬 입력을 처리할 인스턴스에서만(Standalone/ListenHost/AutonomousProxy)
	if (!IsLocallyControlled())
	{
		return false;
	}

	// ASC 필요
	if (!AbilitySystemComponent)
	{
		return false;
	}

	// AttackSequenceAbility는 현재 구현상 RightWeapon 기반(히트디텍션/DA) 의존이 강함
	if (!RightWeapon)
	{
		return false;
	}

	// WeaponDataAsset 준비 확인 (DA가 없으면 Ability 내부 CacheWeaponData도 실패)
	if (!RightWeapon->GetWeaponData())
	{
		return false;
	}

	// HitDetection 인터페이스 준비 확인
	if (!RightWeapon->GetHitDetectionComponent())
	{
		return false;
	}

	return true;
}

void AActionPracticeCharacter::TryAutoActivateAttackSequenceAbility()
{
	// 이미 성공했으면 중복 시도 방지
	if (bAttackSequenceAutoActivated)
	{
		return;
	}

	// 준비 안 됐으면 약간 딜레이 후 재시도 (복제/OnRep 타이밍 흡수)
	if (!IsAttackSequenceAutoActivateReady())
	{
		// 무한 루프 방지 (필요하면 값 조정)
		constexpr int32 MaxRetry = 120; // 0.05s * 120 = 6초
		if (AttackSequenceAutoActivateRetryCount++ >= MaxRetry)
		{
			DEBUG_LOG(TEXT("AttackSequence auto-activate failed: retry limit reached. RightWeapon=%s"),
				*GetNameSafe(RightWeapon.Get()));
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

	// 이제부터 실제 활성화 시도
	GetWorldTimerManager().ClearTimer(AttackSequenceAutoActivateTimer);

	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	// 기존 BeginPlay 로직과 동일한 방식: AssetTags로 AttackSequenceAbility 식별 후 TryActivate
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

		// 이미 활성 상태면 성공 처리
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
