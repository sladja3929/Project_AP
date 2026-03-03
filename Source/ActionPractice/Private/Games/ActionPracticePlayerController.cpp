
#include "Public/Games/ActionPracticePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/ActionPracticeCharacter.h"

// 디버그 로그 활성화/비활성화 (0: 비활성화, 1: 활성화)
#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticePlayerController, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticePlayerController, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void AActionPracticePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Add Input Mapping Contexts
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// ===== Basic Input Actions =====
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::HandleMove);
		}

		if (IA_Look)
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::HandleLook);
		}

		if (IA_LockOn)
		{
			EIC->BindAction(IA_LockOn, ETriggerEvent::Started, this, &AActionPracticePlayerController::HandleToggleLockOn);
		}

		if (IA_WeaponSwitch)
		{
			EIC->BindAction(IA_WeaponSwitch, ETriggerEvent::Started, this, &AActionPracticePlayerController::HandleWeaponSwitch);
		}

		// ===== GAS Ability Input Actions =====
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnJumpPressed);
		}

		if (IA_Sprint)
		{
			EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnSprintPressed);
			EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AActionPracticePlayerController::OnSprintReleased);
		}

		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnCrouchPressed);
		}

		if (IA_Roll)
		{
			EIC->BindAction(IA_Roll, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnRollPressed);
		}

		if (IA_Attack)
		{
			EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnAttackPressed);
		}

		if (IA_ChargeAttack)
		{
			EIC->BindAction(IA_ChargeAttack, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnChargeAttackPressed);
			EIC->BindAction(IA_ChargeAttack, ETriggerEvent::Completed, this, &AActionPracticePlayerController::OnChargeAttackReleased);
		}

		if (IA_Block)
		{
			EIC->BindAction(IA_Block, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnBlockPressed);
			EIC->BindAction(IA_Block, ETriggerEvent::Completed, this, &AActionPracticePlayerController::OnBlockReleased);
		}
	}
}

void AActionPracticePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CachedCharacter = Cast<AActionPracticeCharacter>(InPawn);
}

void AActionPracticePlayerController::OnUnPossess()
{
	CachedCharacter = nullptr;
	Super::OnUnPossess();
}

void AActionPracticePlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	CachedCharacter = Cast<AActionPracticeCharacter>(P);
	DEBUG_LOG(TEXT("AcknowledgePossession: CachedCharacter = %s"), *GetNameSafe(CachedCharacter));
}

void AActionPracticePlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	CachedCharacter = Cast<AActionPracticeCharacter>(GetPawn());
	DEBUG_LOG(TEXT("OnRep_Pawn: CachedCharacter = %s"), *GetNameSafe(CachedCharacter));
}

void AActionPracticePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateLockOnCamera();
}

void AActionPracticePlayerController::HandleMove(const FInputActionValue& Value)
{
	if (CachedCharacter)
	{
		CachedCharacter->ExecuteMove(Value.Get<FVector2D>());
	}
}

void AActionPracticePlayerController::HandleLook(const FInputActionValue& Value)
{
	if (CachedCharacter)
	{
		CachedCharacter->ExecuteLook(Value.Get<FVector2D>());
	}
}

void AActionPracticePlayerController::HandleToggleLockOn()
{
	if (!CachedCharacter) return;

	if (bIsLockOn)
	{
		bIsLockOn = false;
		LockedOnTarget = nullptr;
		CachedCharacter->SetLockedOnTarget(nullptr);
	}
	else
	{
		AActor* NearestTarget = FindNearestTarget();
		if (NearestTarget)
		{
			bIsLockOn = true;
			LockedOnTarget = NearestTarget;
			CachedCharacter->SetLockedOnTarget(NearestTarget);
		}
	}
}

void AActionPracticePlayerController::HandleWeaponSwitch()
{
	if (CachedCharacter)
	{
		CachedCharacter->WeaponSwitch();
	}
}

void AActionPracticePlayerController::HandleGASInputPressed(const UInputAction* InputAction)
{
	if (CachedCharacter)
	{
		CachedCharacter->GASInputPressed(InputAction);
	}
}

void AActionPracticePlayerController::HandleGASInputReleased(const UInputAction* InputAction)
{
	if (CachedCharacter)
	{
		CachedCharacter->GASInputReleased(InputAction);
	}
}

AActor* AActionPracticePlayerController::FindNearestTarget()
{
	if (!CachedCharacter) return nullptr;

	TArray<AActor*> FoundTargets;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Enemy"), FoundTargets);

	AActor* NearestTarget = nullptr;
	float NearestDistance = FLT_MAX;

	for (AActor* PotentialTarget : FoundTargets)
	{
		float Distance = FVector::Dist(CachedCharacter->GetActorLocation(), PotentialTarget->GetActorLocation());
		if (Distance < NearestDistance && Distance < 2000.0f)
		{
			NearestDistance = Distance;
			NearestTarget = PotentialTarget;
		}
	}

	return NearestTarget;
}

void AActionPracticePlayerController::UpdateLockOnCamera()
{
	if (!bIsLockOn) return;

	//타겟이 제거/무효화된 경우 Controller 락온 상태 자동 해제
	if (!IsValid(LockedOnTarget))
	{
		DEBUG_LOG(TEXT("UpdateLockOnCamera: LockOn target is invalid, releasing lock-on"));
		bIsLockOn = false;
		LockedOnTarget = nullptr;
		return;
	}

	if (!CachedCharacter) return;

	const FVector TargetLocation = LockedOnTarget->GetActorLocation();
	const FVector CharacterLocation = CachedCharacter->GetActorLocation();

	//중간점을 바라보게 하여 격렬하게 움직일 때 플레이어와 타겟 모두가 잡히게
	FVector LookAtPoint = (CharacterLocation + TargetLocation) * 0.5f;
	FRotator LookAtRotation = (LookAtPoint - CharacterLocation).Rotation();

	//카메라 위아래 회전 각도 제한
	LookAtRotation.Pitch = FMath::Clamp(LookAtRotation.Pitch, -25.0f, 15.0f);

	FRotator CurrentRotation = GetControlRotation();
	FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, LookAtRotation, GetWorld()->GetDeltaSeconds(), 5.0f);

	SetControlRotation(SmoothedRotation);
}
