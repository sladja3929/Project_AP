
#include "Public/Games/ActionPracticePlayerController.h"
#include "Items/BaseItemDataAsset.h"
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
#include "AI/BossArena.h"
#include "EngineUtils.h"
#include "Interaction/IInteractable.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "UI/MasterHUDWidget.h"
#include "UI/DeathScreenWidget.h"
#include "UI/PauseMenuWidget.h"
#include "Characters/BossCharacter.h"
#include "GAS/AttributeSet/EnemyAttributeSet.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Characters/WeaponManagerComponent.h"
#include "Characters/ItemManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
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
			EIC->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::OnSprintPressed);
			EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AActionPracticePlayerController::OnSprintReleased);
		}

		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnCrouchPressed);
		}

		if (IA_Roll)
		{
			EIC->BindAction(IA_Roll, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::OnRollPressed);
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

		if (IA_SpecialAction)
		{
			EIC->BindAction(IA_SpecialAction, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnSpecialActionPressed);
		}

		//IA_CycleRightWeapon: Shift+휠 Axis1D → HandleCycleWeapon에서 부호로 좌/우 분기
		//IA_CycleLeftWeapon: 바인딩 없음, GAS 키로만 사용
		if (IA_CycleRightWeapon)
		{
			EIC->BindAction(IA_CycleRightWeapon, ETriggerEvent::Triggered, this, &AActionPracticePlayerController::HandleCycleWeapon);
		}

		if (IA_Pause)
		{
			EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &AActionPracticePlayerController::OnPauseInput);
		}
	}

#if !UE_BUILD_SHIPPING
	//디버그: "6" 키로 보스룸 앞 Bonfire 이동 (1·2·3은 무기/적 디버그 트레이스·히트디텍션 토글이 사용)
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AActionPracticePlayerController::OnDebugTeleportToBossBonfire);
	}
#endif
}

void AActionPracticePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	CachedCharacter = Cast<AActionPracticeCharacter>(InPawn);

	//서버: 기본 리스폰 Bonfire 초기화 (아직 설정되지 않은 경우에만)
	if (HasAuthority())
	{
		InitializeDefaultBonfire();
	}

	if (IsLocalController())
	{
		InitializeMasterHUD();
		BindInteractionPromptEvent();
		//PIE 초기화 시 AddDynamic 크래시 방지 — 다음 틱으로 지연
		GetWorldTimerManager().SetTimer(BindHUDTimerHandle, this, &AActionPracticePlayerController::BindPlayerHUDData, 0.01f, false);
		BindDeathStateTagEvent();
		BindRecoveringTagEvent();
	}
}

void AActionPracticePlayerController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(BindHUDTimerHandle);
	bHUDDataBound = false;
	UnbindDeathStateTagEvent();
	UnbindRecoveringTagEvent();
	UnbindInteractionPromptEvent();
	CachedCharacter = nullptr;
	Super::OnUnPossess();
}

void AActionPracticePlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	//타이틀 화면(UIOnly)에서 OpenLevel로 진입 시 입력 모드 복구
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	
	CachedCharacter = Cast<AActionPracticeCharacter>(P);
	DEBUG_LOG(TEXT("AcknowledgePossession: CachedCharacter = %s"), *GetNameSafe(CachedCharacter));

	InitializeMasterHUD();
	BindInteractionPromptEvent();
	GetWorldTimerManager().SetTimer(BindHUDTimerHandle, this, &AActionPracticePlayerController::BindPlayerHUDData, 0.01f, false);
	BindDeathStateTagEvent();
	BindRecoveringTagEvent();
}

void AActionPracticePlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	CachedCharacter = Cast<AActionPracticeCharacter>(GetPawn());
	DEBUG_LOG(TEXT("OnRep_Pawn: CachedCharacter = %s"), *GetNameSafe(CachedCharacter));

	InitializeMasterHUD();
	BindInteractionPromptEvent();
	GetWorldTimerManager().SetTimer(BindHUDTimerHandle, this, &AActionPracticePlayerController::BindPlayerHUDData, 0.01f, false);
	BindDeathStateTagEvent();
	BindRecoveringTagEvent();
}

void AActionPracticePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateLockOnCamera();
}

#pragma region "Non Gas Handler"
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
#pragma endregion

#pragma region "Gas Handler"
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

void AActionPracticePlayerController::OnSprintPressed()
{
	/*if (!CachedCharacter) return;

	UAbilitySystemComponent* ASC = CachedCharacter->GetAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateAbilitySprintingTag()))
	{
		return;
	}*/
	if (bIsSprintPressed) return;
	
	bIsSprintPressed = true;
	HandleGASInputPressed(IA_Sprint);
}

void AActionPracticePlayerController::OnSprintReleased()
{
	bIsSprintPressed = false;
	HandleGASInputReleased(IA_Sprint);
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
#pragma endregion

void AActionPracticePlayerController::SetLastActivatedBonfire(ABonfire* NewBonfire)
{
	LastActivatedBonfire = NewBonfire;
	DEBUG_LOG(TEXT("SetLastActivatedBonfire: %s"), *GetNameSafe(NewBonfire));
}

void AActionPracticePlayerController::InitializeDefaultBonfire()
{
	//이미 설정된 경우 덮어쓰지 않음 (휴식 이후 리스폰 시 재진입 방지)
	if (LastActivatedBonfire.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> FoundBonfires;
	UGameplayStatics::GetAllActorsOfClass(World, ABonfire::StaticClass(), FoundBonfires);

	ABonfire* Fallback = nullptr;
	for (AActor* Actor : FoundBonfires)
	{
		ABonfire* Bonfire = Cast<ABonfire>(Actor);
		if (!Bonfire) continue;

		//bIsDefaultSpawnPoint 우선
		if (Bonfire->IsDefaultSpawnPoint())
		{
			LastActivatedBonfire = Bonfire;
			DEBUG_LOG(TEXT("InitializeDefaultBonfire: Default spawn point set to %s"), *GetNameSafe(Bonfire));
			return;
		}

		//폴백 후보 (첫 번째)
		if (!Fallback)
		{
			Fallback = Bonfire;
		}
	}

	//bIsDefaultSpawnPoint 지정 Bonfire 없으면 첫 번째 Bonfire 사용
	if (Fallback)
	{
		LastActivatedBonfire = Fallback;
		DEBUG_LOG(TEXT("InitializeDefaultBonfire: Fallback to first Bonfire %s"), *GetNameSafe(Fallback));
	}
}

void AActionPracticePlayerController::OnInteractInput()
{
	if (!CachedCharacter) return;

	//휴식 중 일어서기는 InputKey에서 처리 (아무 키), 상호작용은 막음
	if (IsResting()) return;

	UInteractionComponent* InteractionComp = CachedCharacter->FindComponentByClass<UInteractionComponent>();
	if (!InteractionComp) return;

	InteractionComp->TryInteract();
}

bool AActionPracticePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	//마우스 이동·스틱 값 같은 아날로그 축은 제외 (스틱을 크게 기울인 가상 버튼 입력은 포함)
	if (Params.Event == IE_Pressed && !Params.Key.IsAnalog() && IsResting() && !IsPauseKey(Params.Key))
	{
		SendExitRestEvent();
	}

	return Super::InputKey(Params);
}

bool AActionPracticePlayerController::IsResting() const
{
	if (!CachedCharacter) return false;

	const UAbilitySystemComponent* ASC = CachedCharacter->GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateRestingTag());
}

bool AActionPracticePlayerController::IsPauseKey(const FKey& Key) const
{
	if (!IA_Pause) return false;

	const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	return Subsystem && Subsystem->QueryKeysMappedToAction(IA_Pause).Contains(Key);
}

void AActionPracticePlayerController::SendExitRestEvent()
{
	//RestAbility는 ServerInitiated라 WaitInputPress 신호 전달이 불가능하므로 GameplayEvent 방식 사용
	UAbilitySystemComponent* ASC = CachedCharacter ? CachedCharacter->GetAbilitySystemComponent() : nullptr;
	UActionPracticeAbilitySystemComponent* APASC = Cast<UActionPracticeAbilitySystemComponent>(ASC);
	if (!APASC) return;

	FGameplayEventData ExitEventData;
	ExitEventData.EventTag = UGameplayTagsSubsystem::GetAbilityRestTag();
	APASC->HandleGameplayEvent_NetPredicted(UGameplayTagsSubsystem::GetAbilityRestTag(), &ExitEventData);
	DEBUG_LOG(TEXT("SendExitRestEvent: Sent exit rest event"));
}

#pragma region "UI"

void AActionPracticePlayerController::InitializeMasterHUD()
{
	if (!IsLocalController()) return;
	if (!MasterHUDWidgetClass) return;
	if (MasterHUDWidget) return; //이미 생성됨

	MasterHUDWidget = CreateWidget<UMasterHUDWidget>(this, MasterHUDWidgetClass);
	if (MasterHUDWidget)
	{
		MasterHUDWidget->AddToViewport();
		//AddToViewport 이후 호출 — UMG 초기화 완료 후 자식 위젯 생성
		MasterHUDWidget->CreateChildWidgets();

		//계속하기 버튼도 ESC 토글과 같은 입력 모드 복원 경로를 거치도록 PC가 처리
		if (UPauseMenuWidget* PauseMenu = MasterHUDWidget->GetPauseMenuWidget())
		{
			PauseMenu->OnResumeRequested.AddUObject(this, &AActionPracticePlayerController::HandlePauseMenuResumeRequested);
		}

		DEBUG_LOG(TEXT("MasterHUDWidget created and added to viewport"));
	}
}

void AActionPracticePlayerController::BindPlayerHUDData()
{
	if (bHUDDataBound) return;
	if (!MasterHUDWidget) return;
	if (!CachedCharacter) return;

	UActionPracticeAttributeSet* AttrSet = CachedCharacter->GetAttributeSet();
	UWeaponManagerComponent* WeaponMgr = CachedCharacter->GetWeaponManagerComponent();
	UItemManagerComponent* ItemMgr = CachedCharacter->GetItemManagerComponent();

	MasterHUDWidget->BindPlayerData(AttrSet, WeaponMgr, ItemMgr);
	bHUDDataBound = true;
	DEBUG_LOG(TEXT("BindPlayerHUDData: Bound to %s"), *GetNameSafe(CachedCharacter));
}

void AActionPracticePlayerController::ShowBossHealth(ABossCharacter* Boss)
{
	if (!MasterHUDWidget || !Boss) return;

	UEnemyAttributeSet* BossAttrSet = Boss->GetAttributeSet();
	MasterHUDWidget->ShowBossHealth(BossAttrSet, Boss->EnemyName);
	DEBUG_LOG(TEXT("ShowBossHealth: %s"), *Boss->EnemyName.ToString());
}

void AActionPracticePlayerController::HideBossHealth()
{
	if (!MasterHUDWidget) return;

	MasterHUDWidget->HideBossHealth();
	DEBUG_LOG(TEXT("HideBossHealth"));
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
	if (!MasterHUDWidget || !CachedDeathUIASC) return;

	if (CachedDeathUIASC->HasMatchingGameplayTag(StateDeadTag))
	{
		MasterHUDWidget->SetDeathScreenVisibility(true);
	}
	else
	{
		MasterHUDWidget->SetDeathScreenVisibility(false);
	}
}

void AActionPracticePlayerController::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (!MasterHUDWidget) return;

	if (NewCount > 0)
	{
		MasterHUDWidget->HandleDeadStateStart();
		DEBUG_LOG(TEXT("HandleDeadTagChanged: Show (Count=%d)"), NewCount);
	}
	else
	{
		MasterHUDWidget->HandleDeadStateFinish();
		DEBUG_LOG(TEXT("HandleDeadTagChanged: Hide"));
	}
}

void AActionPracticePlayerController::BindInteractionPromptEvent()
{
	if (!CachedCharacter) return;

	UInteractionComponent* InteractionComp = CachedCharacter->FindComponentByClass<UInteractionComponent>();
	if (!InteractionComp) return;

	InteractionComp->OnInteractableChanged.RemoveDynamic(this, &AActionPracticePlayerController::OnInteractableChanged);
	InteractionComp->OnInteractableChanged.AddDynamic(this, &AActionPracticePlayerController::OnInteractableChanged);
	DEBUG_LOG(TEXT("BindInteractionPromptEvent: Bound"));
}

void AActionPracticePlayerController::UnbindInteractionPromptEvent()
{
	if (!CachedCharacter) return;

	UInteractionComponent* InteractionComp = CachedCharacter->FindComponentByClass<UInteractionComponent>();
	if (!InteractionComp) return;

	InteractionComp->OnInteractableChanged.RemoveDynamic(this, &AActionPracticePlayerController::OnInteractableChanged);
	DEBUG_LOG(TEXT("UnbindInteractionPromptEvent: Unbound"));
}

void AActionPracticePlayerController::OnInteractableChanged(AActor* NewInteractable)
{
	if (!MasterHUDWidget) return;

	if (NewInteractable)
	{
		IInteractable* InteractableInterface = Cast<IInteractable>(NewInteractable);
		if (InteractableInterface)
		{
			const FText PromptText = InteractableInterface->GetInteractionPrompt();
			MasterHUDWidget->ShowInteractionPrompt(PromptText);
			bIsInteractionPromptVisible = true;

			//현재 Recovering 태그 상태 즉시 반영 (UI 뜰 때 이미 태그가 있는 경우)
			if (CachedRecoveringUIASC)
			{
				const bool bIsRecovering = CachedRecoveringUIASC->HasMatchingGameplayTag(StateRecoveringLocalTag);
				MasterHUDWidget->SetInteractionPromptDimmed(bIsRecovering);
			}

			DEBUG_LOG(TEXT("OnInteractableChanged: Show prompt — %s"), *PromptText.ToString());
			return;
		}
	}

	MasterHUDWidget->HideInteractionPrompt();
	bIsInteractionPromptVisible = false;
	DEBUG_LOG(TEXT("OnInteractableChanged: Hide prompt"));
}

void AActionPracticePlayerController::BindRecoveringTagEvent()
{
	if (!CachedCharacter) return;

	UAbilitySystemComponent* ASC = CachedCharacter->GetAbilitySystemComponent();
	if (!ASC) return;

	if (!StateRecoveringLocalTag.IsValid())
	{
		StateRecoveringLocalTag = UGameplayTagsSubsystem::GetStateRecoveringLocalTag();
	}
	if (!StateRecoveringLocalTag.IsValid()) return;

	CachedRecoveringUIASC = ASC;
	RecoveringTagChangedHandle = ASC->RegisterGameplayTagEvent(StateRecoveringLocalTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AActionPracticePlayerController::HandleRecoveringTagChanged);

	DEBUG_LOG(TEXT("BindRecoveringTagEvent: Bound to ASC %s"), *GetNameSafe(ASC));
}

void AActionPracticePlayerController::UnbindRecoveringTagEvent()
{
	if (CachedRecoveringUIASC && RecoveringTagChangedHandle.IsValid())
	{
		CachedRecoveringUIASC->UnregisterGameplayTagEvent(RecoveringTagChangedHandle, StateRecoveringLocalTag, EGameplayTagEventType::NewOrRemoved);
		RecoveringTagChangedHandle.Reset();
		DEBUG_LOG(TEXT("UnbindRecoveringTagEvent: Unbound"));
	}
	CachedRecoveringUIASC = nullptr;
}

void AActionPracticePlayerController::HandleRecoveringTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (!MasterHUDWidget) return;
	if (!bIsInteractionPromptVisible) return;

	MasterHUDWidget->SetInteractionPromptDimmed(NewCount > 0);
	DEBUG_LOG(TEXT("HandleRecoveringTagChanged: Dimmed=%s (Count=%d)"), NewCount > 0 ? TEXT("true") : TEXT("false"), NewCount);
}

void AActionPracticePlayerController::OnPauseInput()
{
	if (!IsLocalController()) return;
	if (!MasterHUDWidget) return;

	MasterHUDWidget->TogglePauseMenu();
	ApplyPauseMenuInputMode();
}

void AActionPracticePlayerController::HandlePauseMenuResumeRequested()
{
	if (!IsLocalController()) return;
	if (!MasterHUDWidget) return;

	MasterHUDWidget->HidePauseMenu();
	ApplyPauseMenuInputMode();
}

void AActionPracticePlayerController::ApplyPauseMenuInputMode()
{
	if (!MasterHUDWidget) return;

	if (MasterHUDWidget->IsPauseMenuVisible())
	{
		//메뉴 열림 — 마우스 커서 표시 + 게임/UI 겸용 입력
		bShowMouseCursor = true;
		SetInputMode(FInputModeGameAndUI());
		DEBUG_LOG(TEXT("Pause Menu Opened"));
	}
	else
	{
		//메뉴 닫힘 — 마우스 커서 숨김 + 게임 전용 입력
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		DEBUG_LOG(TEXT("Pause Menu Closed"));
	}
}

#pragma endregion

#pragma region "Debug"

void AActionPracticePlayerController::OnDebugTeleportToBossBonfire()
{
	Server_DebugTeleportToBossBonfire();
}

void AActionPracticePlayerController::Server_DebugTeleportToBossBonfire_Implementation()
{
#if !UE_BUILD_SHIPPING
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();

	if (!ControlledPawn || !World)
	{
		return;
	}

	//보스룸이 있는 맵에서만 동작 (보스룸에 가장 가까운 Bonfire = 보스 직전 휴식 지점)
	TActorIterator<ABossArena> ArenaIt(World);

	if (!ArenaIt)
	{
		DEBUG_LOG(TEXT("DebugTeleportToBossBonfire: No BossArena in this map"));
		return;
	}

	const FVector ArenaLocation = ArenaIt->GetActorLocation();
	ABonfire* NearestBonfire = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();

	for (TActorIterator<ABonfire> It(World); It; ++It)
	{
		const double DistSq = FVector::DistSquared(It->GetActorLocation(), ArenaLocation);

		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestBonfire = *It;
		}
	}

	if (!NearestBonfire)
	{
		DEBUG_LOG(TEXT("DebugTeleportToBossBonfire: No Bonfire in this map"));
		return;
	}

	//사망 중에는 이동하지 않음 (리스폰 흐름과 충돌 방지)
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn);
	const FGameplayTag& DeadTag = UGameplayTagsSubsystem::GetStateDeadTag();

	if (ASC && DeadTag.IsValid() && ASC->HasMatchingGameplayTag(DeadTag))
	{
		return;
	}

	//낙하 등 남은 속도 제거 후, 리스폰과 같은 위치·방식으로 이동하고 카메라도 리스폰 방향으로 맞춤
	if (const ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
	{
		ControlledCharacter->GetCharacterMovement()->StopMovementImmediately();
	}

	ControlledPawn->SetActorTransform(NearestBonfire->GetRespawnTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	ClientSetRotation(NearestBonfire->GetRespawnRotation());
	DEBUG_LOG(TEXT("DebugTeleportToBossBonfire: %s"), *GetNameSafe(NearestBonfire));
#endif
}

#pragma endregion

void AActionPracticePlayerController::Client_NotifyItemAcquired_Implementation(UBaseItemDataAsset* InItemDA, int32 InCount)
{
	if (!MasterHUDWidget) return;
	if (!InItemDA) return;

	MasterHUDWidget->ShowItemAcquisition(InItemDA, InCount);
	DEBUG_LOG(TEXT("Client_NotifyItemAcquired: %s x%d"), *InItemDA->DisplayName.ToString(), InCount);
}

void AActionPracticePlayerController::UpdateLockOnCamera()
{
	if (!CachedCharacter) return;

	ULockOnComponent* LockOnComp = CachedCharacter->GetLockOnComponent();
	if (!LockOnComp || !LockOnComp->IsLockedOn()) return;

	//타겟 무효 판정과 해제는 LockOnComponent의 유지 타이머로 일원화했다. 여기서는 카메라만 처리한다
	AActor* Target = LockOnComp->GetLockOnTarget();
	if (!IsValid(Target)) return;

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
