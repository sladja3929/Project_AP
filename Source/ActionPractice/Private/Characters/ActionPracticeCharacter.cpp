#include "Public/Characters/ActionPracticeCharacter.h"
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
	StateRecoveringTag = UGameplayTagsSubsystem::GetStateRecoveringTag();
	StateAbilitySprintingTag = UGameplayTagsSubsystem::GetStateAbilitySprintingTag();
	StateAbilityAttackingTag = UGameplayTagsSubsystem::GetStateAbilityAttackingTag();
	AbilityAttackTag = UGameplayTagsSubsystem::GetAbilityAttackTag();

	if (!StateRecoveringTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateRecoveringTag is not valid"));
	}
	if (!StateAbilitySprintingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilitySprintingTag is not valid"));
	}
	if (!StateAbilityAttackingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityAttackingTag is not valid"));
	}
	if (!AbilityAttackTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityAttackTag is not valid"));
	}

	InitializeAbilitySystem();

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
}

void AActionPracticeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLockOnCamera();
}

void AActionPracticeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// ===== Basic Input Actions (Direct Function Binding) =====
		if (IA_Move)
		{
			EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AActionPracticeCharacter::Move);
		}
        
		if (IA_Look)
		{
			EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AActionPracticeCharacter::Look);
		}

		if(IA_LockOn)
		{
			EnhancedInputComponent->BindAction(IA_LockOn, ETriggerEvent::Started, this, &AActionPracticeCharacter::ToggleLockOn);
		}

		if(IA_WeaponSwitch)
		{
			EnhancedInputComponent->BindAction(IA_WeaponSwitch, ETriggerEvent::Started, this, &AActionPracticeCharacter::WeaponSwitch);
		}

		// ===== GAS Ability Input Actions =====
		// Jump
		if (IA_Jump)
		{
			EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnJumpInput);
		}
        
		// Sprint (Hold)
		if (IA_Sprint)
		{
			EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnSprintInput);
			EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AActionPracticeCharacter::OnSprintInputReleased);
		}
        
		// Crouch
		if (IA_Crouch)
		{
			EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnCrouchInput);
		}
        
		// Roll
		if (IA_Roll)
		{
			EnhancedInputComponent->BindAction(IA_Roll, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnRollInput);
		}
        
		// Attack
		if (IA_Attack)
		{
			EnhancedInputComponent->BindAction(IA_Attack, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnAttackInput);
		}

		//Hold
		if (IA_ChargeAttack)
		{
			EnhancedInputComponent->BindAction(IA_ChargeAttack, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnChargeAttackInput);
			EnhancedInputComponent->BindAction(IA_ChargeAttack, ETriggerEvent::Completed, this, &AActionPracticeCharacter::OnChargeAttackReleased);
		}
		
		// Block (Hold)
		if (IA_Block)
		{
			EnhancedInputComponent->BindAction(IA_Block, ETriggerEvent::Started, this, &AActionPracticeCharacter::OnBlockInput);
			EnhancedInputComponent->BindAction(IA_Block, ETriggerEvent::Completed, this, &AActionPracticeCharacter::OnBlockInputReleased);
		}
    }
}

#pragma region "Move Functions"
void AActionPracticeCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	//리커버리가 끝나면 어빌리티 중단
	if (MovementVector.Size() > 0.1f)
	{
		CancelActionForMove();
	}

	bool bIsRecovering = AbilitySystemComponent->HasMatchingGameplayTag(StateRecoveringTag);

	if (Controller != nullptr && !bIsRecovering)
	{
		bool bIsSprinting = AbilitySystemComponent->HasMatchingGameplayTag(StateAbilitySprintingTag);

		//락온 상태에서 걸을 때: Strafe 이동
		if(!bIsSprinting && bIsLockOn && LockedOnTarget)
		{
			//Strafe 이동 설정
			GetCharacterMovement()->bOrientRotationToMovement = false;
			GetCharacterMovement()->bUseControllerDesiredRotation = false;

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

			//캐릭터가 타겟을 바라보도록 회전
			SetActorRotation(TargetRotation);
		}

		else //일반적인 회전 이동 (락온 없음 or 락온+달리기)
		{
			//일반 회전 이동 설정
			GetCharacterMovement()->bOrientRotationToMovement = true;
			GetCharacterMovement()->bUseControllerDesiredRotation = false;

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
	APlayerController* PC = GetController<APlayerController>();
	if (PC && IA_Move)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = 
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			//특정 액션의 현재 값 조회
			FInputActionValue ActionValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Move);
			return ActionValue.Get<FVector2D>();
		}
	}
	return FVector2D::ZeroVector;
}

bool AActionPracticeCharacter::IsBlockInputPressed() const
{
	APlayerController* PC = GetController<APlayerController>();
	if (PC && IA_Block)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// IA_Block 액션의 현재 값 조회
			FInputActionValue ActionValue = Subsystem->GetPlayerInput()->GetActionValue(IA_Block);
			return ActionValue.Get<bool>();
		}
	}
	return false;
}

void AActionPracticeCharacter::RotateCharacterToInputDirection(float RotateTime, bool bIgnoreLockOn)
{
	//락온 상태면 락온 대상 방향으로
	if (!bIgnoreLockOn && bIsLockOn)
	{
		if (!LockedOnTarget) return;

		//BaseCharacter의 RotateToPosition 호출
		RotateToPosition(LockedOnTarget->GetActorLocation(), RotateTime);
	}

	//아니면 입력 방향으로
	else
	{
		//현재 입력 값 가져오기
		FVector2D MovementInput = GetCurrentMovementInput();
		if (MovementInput.IsZero()) return;
		
		//카메라의 Yaw 회전만 가져오기
		FRotator CameraRotation = FollowCamera->GetComponentRotation();
		FRotator CameraYaw = FRotator(0.0f, CameraRotation.Yaw, 0.0f);

		//입력 벡터를 3D로 변환
		FVector InputDirection = FVector(MovementInput.Y, MovementInput.X, 0.0f);

		//카메라 기준으로 입력 방향 변환
		FVector WorldDirection = CameraYaw.RotateVector(InputDirection);
		WorldDirection.Normalize();

		//목표 회전 계산
		FRotator TargetRotation = FRotator(0.0f, WorldDirection.Rotation().Yaw, 0.0f);

		//BaseCharacter의 RotateToRotation 호출
		RotateToRotation(TargetRotation, RotateTime);
	}
}

void AActionPracticeCharacter::CancelActionForMove()
{
	if (!AbilitySystemComponent)
	{
		return;
	}
    
	//Attack 어빌리티가 활성화되어 있는지 확인
	bool bHasActiveAttackAbility = AbilitySystemComponent->HasMatchingGameplayTag(StateAbilityAttackingTag);

	if (bHasActiveAttackAbility)
	{
		//State.Recovering 태그가 없으면 어빌리티 캔슬 가능 (ActionRecoveryEnd 이후)
		if (!AbilitySystemComponent->HasMatchingGameplayTag(StateRecoveringTag))
		{
			//Ability.Attack 태그를 가진 어빌리티 취소
			FGameplayTagContainer CancelTags;
			CancelTags.AddTag(AbilityAttackTag);
			AbilitySystemComponent->CancelAbilities(&CancelTags);
			DEBUG_LOG(TEXT("Attack Ability Cancelled by Move Input"));
		}
		else
		{
			DEBUG_LOG(TEXT("Attack Ability is in Recovering state - cannot cancel"));
		}
	}
}
#pragma endregion

#pragma region "Look Functions"
void AActionPracticeCharacter::Look(const FInputActionValue& Value)
{
	if (Controller == nullptr) return;
	if(bIsLockOn && LockedOnTarget) return;
	
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AActionPracticeCharacter::ToggleLockOn()
{
	if (bIsLockOn)
	{
		bIsLockOn = false;
		LockedOnTarget = nullptr;

		//일반 이동 회전으로 복원
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;

		if (CameraBoom)
		{
			//필요하면 카메라 설정 복원
		}

		DEBUG_LOG(TEXT("Lock-On Released"));
	}
	else
	{
		AActor* NearestTarget = FindNearestTarget();
		if (NearestTarget)
		{
			bIsLockOn = true;
			LockedOnTarget = NearestTarget;

			DEBUG_LOG(TEXT("Lock-On Target: %s"), *NearestTarget->GetName());
		}
		else
		{
			DEBUG_LOG(TEXT("No valid target found for Lock-On"));
		}
	}
}

AActor* AActionPracticeCharacter::FindNearestTarget()
{
	TArray<AActor*> FoundTargets;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Enemy"), FoundTargets);
    
	AActor* NearestTarget = nullptr;
	float NearestDistance = FLT_MAX;
    
	for (AActor* PotentialTarget : FoundTargets)
	{
		float Distance = FVector::Dist(GetActorLocation(), PotentialTarget->GetActorLocation());
		if (Distance < NearestDistance && Distance < 2000.0f)
		{
			NearestDistance = Distance;
			NearestTarget = PotentialTarget;
		}
	}
    
	return NearestTarget;
}

void AActionPracticeCharacter::UpdateLockOnCamera()
{
    if (bIsLockOn && LockedOnTarget && Controller && CameraBoom)
    {
        const FVector TargetLocation = LockedOnTarget->GetActorLocation();
        const FVector CharacterLocation = GetActorLocation();

    	//중간점을 바라보게 하여 격렬하게 움직일 때 플레이어와 타겟 모두가 잡히게
        FVector LookAtPoint = (CharacterLocation + TargetLocation) * 0.5f;
        FRotator LookAtRotation = (LookAtPoint - CharacterLocation).Rotation();

    	//카메라 위아래 회전 각도 제한
        LookAtRotation.Pitch = FMath::Clamp(LookAtRotation.Pitch, -25.0f, 15.0f);
    	
        FRotator CurrentRotation = Controller->GetControlRotation();
        FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, LookAtRotation, GetWorld()->GetDeltaSeconds(), 5.0f);
    	
        Controller->SetControlRotation(SmoothedRotation);
    }
}
#pragma endregion

#pragma region "Weapon Functions"

TScriptInterface<IHitDetectionInterface> AActionPracticeCharacter::GetHitDetectionInterface() const
{
	return RightWeapon->GetHitDetectionComponent();
}

void AActionPracticeCharacter::WeaponSwitch()
{
}

void AActionPracticeCharacter::EquipWeapon(TSubclassOf<AWeapon> NewWeaponClass, bool bIsLeftHand, bool bIsTwoHanded)
{
	// 서버에서만 실행 (싱글플레이어에서는 HasAuthority()가 항상 true)
	if (!HasAuthority())
	{
		return;
	}

	if (!NewWeaponClass) return;

	if(bIsTwoHanded) UnequipWeapon(!bIsLeftHand);
	UnequipWeapon(bIsLeftHand);

	// 새 무기 스폰
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

#pragma region "GAS Input Functions"
void AActionPracticeCharacter::OnJumpInput()
{
	GASInputPressed(IA_Jump);
}

void AActionPracticeCharacter::OnSprintInput()
{
	GASInputPressed(IA_Sprint);
}

void AActionPracticeCharacter::OnSprintInputReleased()
{
	GASInputReleased(IA_Sprint);
}

void AActionPracticeCharacter::OnCrouchInput()
{
	GASInputPressed(IA_Crouch);
}

void AActionPracticeCharacter::OnRollInput()
{
	GASInputPressed(IA_Roll);
}

void AActionPracticeCharacter::OnAttackInput()
{
	GASInputPressed(IA_Attack);
}

void AActionPracticeCharacter::OnBlockInput()
{
	GASInputPressed(IA_Block);
}

void AActionPracticeCharacter::OnBlockInputReleased()
{
	GASInputReleased(IA_Block);
}

void AActionPracticeCharacter::OnChargeAttackInput()
{
	GASInputPressed(IA_ChargeAttack);
}

void AActionPracticeCharacter::OnChargeAttackReleased()
{
	GASInputReleased(IA_ChargeAttack);
}

#pragma endregion

#pragma region "GAS Functions"
void AActionPracticeCharacter::InitializeAbilitySystem()
{
	Super::InitializeAbilitySystem();
}

void AActionPracticeCharacter::CreateAbilitySystemComponent()
{
	AbilitySystemComponent = CreateDefaultSubobject<UActionPracticeAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
}

void AActionPracticeCharacter::CreateAttributeSet()
{
	AttributeSet = CreateDefaultSubobject<UActionPracticeAttributeSet>(TEXT("AttributeSet"));
}

void AActionPracticeCharacter::GASInputPressed(const UInputAction* InputAction)
{
	if (!AbilitySystemComponent || !InputAction) return;

	TArray<FGameplayAbilitySpec*> TryActivateSpecs = FindAbilitySpecsWithInputAction(InputAction);
	if (TryActivateSpecs.IsEmpty()) return;
	
	InputBufferComponent->bBufferActionReleased = false;
	//다른 어빌리티가 수행중이고 입력 저장 가능할 때는 버퍼로 전달, Ability->InputPressed는 버퍼 이외의 구간에서만 사용
	if (InputBufferComponent->bCanBufferInput)
	{
		DEBUG_LOG(TEXT("Character: Buffer"));
		InputBufferComponent->BufferNextAction(InputAction);
	}

	else
	{
		for (auto& Spec : TryActivateSpecs)
		{
			if (Spec->IsActive())
			{
				Spec->InputPressed = true;
				AbilitySystemComponent->AbilitySpecInputPressed(*Spec);
			}

			else
			{
				Spec->InputPressed = true;
				AbilitySystemComponent->TryActivateAbility(Spec->Handle);
			}
		}
	}
}

void AActionPracticeCharacter::GASInputReleased(const UInputAction* InputAction)
{
	if (!AbilitySystemComponent || !InputAction) return;

	TArray<FGameplayAbilitySpec*> TryActivateSpecs = FindAbilitySpecsWithInputAction(InputAction);
	if (TryActivateSpecs.IsEmpty()) return;
	
	//다른 어빌리티가 수행중이고 입력 저장 가능할 때는 버퍼로 전달, Ability->InputPressed는 버퍼 이외의 구간에서만 사용
	if (InputBufferComponent->bCanBufferInput)
	{
		DEBUG_LOG(TEXT("Character: UnBuffer"));
		InputBufferComponent->UnBufferHoldAction(InputAction);
	}
	
	else
	{
		for (auto& Spec : TryActivateSpecs)
		{
			if (Spec->IsActive())
			{
				Spec->InputPressed = false;
				AbilitySystemComponent->AbilitySpecInputReleased(*Spec);
			}
		}
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
		// 무기 타입에 따라 소켓 이름 결정
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
}

void AActionPracticeCharacter::OnRep_RightWeapon()
{
	if (RightWeapon && GetMesh())
	{
		// 무기 타입에 따라 소켓 이름 결정
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
}
#pragma endregion
