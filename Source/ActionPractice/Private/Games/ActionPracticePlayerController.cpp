
#include "Public/Games/ActionPracticePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/LockOnComponent.h"
#include "Characters/ItemManagerComponent.h"
#include "Characters/InteractionComponent.h"
#include "Interaction/Bonfire.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "UI/DeathScreenWidget.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"

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

		if (IA_Interact)
		{
			EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnInteractInput);
		}

		if (IA_CycleQuickSlot)
		{
			EIC->BindAction(IA_CycleQuickSlot, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::HandleCycleQuickSlot);
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

		if (IA_UseItem)
		{
			EIC->BindAction(IA_UseItem, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnUseItemPressed);
		}

		//IA_CycleRightWeapon: Shift+휠 Axis1D → HandleCycleWeapon에서 부호로 좌/우 분기
		//IA_CycleLeftWeapon: 바인딩 없음, GAS 키로만 사용
		if (IA_CycleRightWeapon)
		{
			EIC->BindAction(IA_CycleRightWeapon, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::HandleCycleWeapon);
		}
	}
}

void AActionPracticePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CachedCharacter = Cast<AActionPracticeCharacter>(InPawn);

	if (IsLocalController())
	{
		InitializeDeathScreenWidget();
		BindDeathStateTagEvent();
	}
}

void AActionPracticePlayerController::OnUnPossess()
{
	UnbindDeathStateTagEvent();
	CachedCharacter = nullptr;
	Super::OnUnPossess();
}

void AActionPracticePlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	CachedCharacter = Cast<AActionPracticeCharacter>(P);
	DEBUG_LOG(TEXT("AcknowledgePossession: CachedCharacter = %s"), *GetNameSafe(CachedCharacter));

	InitializeDeathScreenWidget();
	BindDeathStateTagEvent();
}

void AActionPracticePlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	CachedCharacter = Cast<AActionPracticeCharacter>(GetPawn());
	DEBUG_LOG(TEXT("OnRep_Pawn: CachedCharacter = %s"), *GetNameSafe(CachedCharacter));

	InitializeDeathScreenWidget();
	BindDeathStateTagEvent();
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

	ULockOnComponent* LockOnComp = CachedCharacter->GetLockOnComponent();
	if (!LockOnComp) return;

	LockOnComp->ToggleLockOn();
}

void AActionPracticePlayerController::HandleCycleQuickSlot(const FInputActionValue& Value)
{
	//휠 아래(음수): 퀵슬롯 순환
	//필요 시 양수(위) 방향도 별도 처리
	if (Value.Get<float>() >= 0.f) return;

	if (CachedCharacter)
	{
		UItemManagerComponent* ItemManager = CachedCharacter->GetItemManagerComponent();
		if (ItemManager)
		{
			ItemManager->CycleQuickSlot();
		}
	}
}

void AActionPracticePlayerController::HandleCycleWeapon(const FInputActionValue& Value)
{
	//Shift+휠 위(양수): 오른손 무기 순환
	//Shift+휠 아래(음수): 왼손 무기 순환
	const float ScrollValue = Value.Get<float>();
	if (ScrollValue > 0.f)
	{
		HandleGASInputPressed(IA_CycleRightWeapon);
	}
	
	else if (ScrollValue < 0.f)
	{
		HandleGASInputPressed(IA_CycleLeftWeapon);
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

void AActionPracticePlayerController::SetLastActivatedBonfire(ABonfire* NewBonfire)
{
	LastActivatedBonfire = NewBonfire;
	DEBUG_LOG(TEXT("SetLastActivatedBonfire: %s"), *GetNameSafe(NewBonfire));
}

void AActionPracticePlayerController::OnInteractInput()
{
	if (!CachedCharacter) return;

	//휴식 중이면 Ability.Rest 이벤트를 전송해 RestAbility의 WaitGameplayEvent를 트리거
	//RestAbility는 ServerInitiated라 WaitInputPress 신호 전달이 불가능하므로 GameplayEvent 방식 사용
	UActionPracticeAbilitySystemComponent* APASC = Cast<UActionPracticeAbilitySystemComponent>(CachedCharacter->GetAbilitySystemComponent());
	if (APASC && APASC->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateRestingTag()))
	{
		FGameplayEventData ExitEventData;
		ExitEventData.EventTag = UGameplayTagsSubsystem::GetAbilityRestTag();
		APASC->HandleGameplayEvent_NetPredicted(UGameplayTagsSubsystem::GetAbilityRestTag(), &ExitEventData);
		DEBUG_LOG(TEXT("OnInteractInput: Sent exit rest event"));
		return;
	}

	UInteractionComponent* InteractionComp = CachedCharacter->FindComponentByClass<UInteractionComponent>();
	if (!InteractionComp) return;

	InteractionComp->TryInteract();
}

#pragma region "Death UI"

void AActionPracticePlayerController::InitializeDeathScreenWidget()
{
	if (!IsLocalController()) return;
	if (!DeathScreenWidgetClass) return;
	if (DeathScreenWidget) return; //이미 생성됨

	DeathScreenWidget = CreateWidget<UDeathScreenWidget>(this, DeathScreenWidgetClass);
	if (DeathScreenWidget)
	{
		DeathScreenWidget->AddToViewport();
		DeathScreenWidget->HandleDeadStateFinish();
		DEBUG_LOG(TEXT("InitializeDeathScreenWidget: Created and hidden"));
	}
}

void AActionPracticePlayerController::BindDeathStateTagEvent()
{
	UnbindDeathStateTagEvent();

	if (!CachedCharacter) return;

	UAbilitySystemComponent* ASC = CachedCharacter->GetAbilitySystemComponent();
	if (!ASC) return;

	if (!StateDeadTag.IsValid())
	{
		StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();
	}
	if (!StateDeadTag.IsValid()) return;

	CachedDeathUIASC = ASC;
	DeadTagChangedHandle = ASC->RegisterGameplayTagEvent(StateDeadTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AActionPracticePlayerController::HandleDeadTagChanged);

	//RefreshDeathScreenVisibilityFromASC();
	DEBUG_LOG(TEXT("BindDeathStateTagEvent: Bound to ASC %s"), *GetNameSafe(ASC));
}

void AActionPracticePlayerController::UnbindDeathStateTagEvent()
{
	if (CachedDeathUIASC && DeadTagChangedHandle.IsValid())
	{
		CachedDeathUIASC->UnregisterGameplayTagEvent(DeadTagChangedHandle, StateDeadTag, EGameplayTagEventType::NewOrRemoved);
		DeadTagChangedHandle.Reset();
		DEBUG_LOG(TEXT("UnbindDeathStateTagEvent: Unbound"));
	}
	CachedDeathUIASC = nullptr;
}

void AActionPracticePlayerController::RefreshDeathScreenVisibilityFromASC()
{
	if (!DeathScreenWidget || !CachedDeathUIASC) return;

	if (CachedDeathUIASC->HasMatchingGameplayTag(StateDeadTag))
	{
		DeathScreenWidget->SetDeathScreenVisibility(true);
	}
	else
	{
		DeathScreenWidget->SetDeathScreenVisibility(false);
	}
}

void AActionPracticePlayerController::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (!DeathScreenWidget) return;

	if (NewCount > 0)
	{
		DeathScreenWidget->HandleDeadStateStart();
		DEBUG_LOG(TEXT("HandleDeadTagChanged: Show (Count=%d)"), NewCount);
	}
	else
	{
		DeathScreenWidget->HandleDeadStateFinish();
		DEBUG_LOG(TEXT("HandleDeadTagChanged: Hide"));
	}
}

#pragma endregion

void AActionPracticePlayerController::UpdateLockOnCamera()
{
	if (!CachedCharacter) return;

	ULockOnComponent* LockOnComp = CachedCharacter->GetLockOnComponent();
	if (!LockOnComp || !LockOnComp->IsLockedOn()) return;

	AActor* Target = LockOnComp->GetLockOnTarget();
	if (!IsValid(Target))
	{
		DEBUG_LOG(TEXT("UpdateLockOnCamera: LockOn target is invalid, releasing lock-on"));
		LockOnComp->SetLockedOnTarget(nullptr);
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
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
