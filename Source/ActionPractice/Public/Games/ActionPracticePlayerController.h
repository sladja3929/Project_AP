#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ActionPracticePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class AActionPracticeCharacter;
class ABonfire;
class UMasterHUDWidget;
class UAbilitySystemComponent;
class ABossCharacter;
class UBaseItemDataAsset;

/**
 *  PlayerController for ActionPractice
 *  Manages input mappings and input bindings
 */
UCLASS(abstract)
class AActionPracticePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void OnRep_Pawn() override;
	virtual void PlayerTick(float DeltaTime) override;

	FORCEINLINE UInputAction* GetIA_Move() const { return IA_Move; }
	FORCEINLINE UInputAction* GetIA_Block() const { return IA_Block; }

	void SetLastActivatedBonfire(ABonfire* NewBonfire);
	FORCEINLINE ABonfire* GetLastActivatedBonfire() const { return LastActivatedBonfire.Get(); }

	//보스 캐릭터에서 호출 — 로컬 HUD에 보스 HP바 표시/숨김
	void ShowBossHealth(ABossCharacter* Boss);
	void HideBossHealth();

	//아이템 획득 알림을 소유 클라이언트에 전달 — 서버에서 호출
	UFUNCTION(Client, Reliable)
	void Client_NotifyItemAcquired(UBaseItemDataAsset* InItemDA, int32 InCount);

#pragma endregion

protected:
#pragma region "Protected Variables"

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input", meta = (AllowPrivateAccess = "true"))
	TArray<UInputMappingContext*> DefaultMappingContexts;

	// ====== Input Actions ======
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Jump = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Look = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_LockOn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Interact = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Sprint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Crouch = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Roll = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Attack = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Block = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_CycleRightWeapon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_CycleLeftWeapon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_ChargeAttack = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_UseItem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_SpecialAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_CycleQuickSlot = nullptr;

	// ===== UI =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UMasterHUDWidget> MasterHUDWidgetClass = nullptr;

	UPROPERTY()
	TObjectPtr<UMasterHUDWidget> MasterHUDWidget = nullptr;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedDeathUIASC = nullptr;

	FGameplayTag StateDeadTag;
	FDelegateHandle DeadTagChangedHandle;

	//BindPlayerHUDData 지연 호출용 — 동일 핸들 재사용으로 중복 방지
	FTimerHandle BindHUDTimerHandle;

	//AcknowledgePossession+OnRep_Pawn 타이밍 경쟁으로 인한 중복 바인딩 방지
	bool bHUDDataBound = false;

	// ===== Interaction Prompt Dimming =====
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedRecoveringUIASC = nullptr;

	FGameplayTag StateRecoveringLocalTag;
	FDelegateHandle RecoveringTagChangedHandle;

	//프롬프트 표시 상태 추적
	bool bIsInteractionPromptVisible = false;

#pragma endregion

#pragma region "Protected Functions"

	/** Input mapping context + action binding setup */
	virtual void SetupInputComponent() override;

	// ===== Input Handlers =====
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleToggleLockOn();
	
	//마우스 휠 스크롤 (Axis1D): 양수=위, 음수=아래
	void HandleCycleQuickSlot(const FInputActionValue& Value);
	
	//Shift+휠: 양수=RightWeapon, 음수=LeftWeapon
	void HandleCycleWeapon(const FInputActionValue& Value);
	void HandleGASInputPressed(const UInputAction* InputAction);
	void HandleGASInputReleased(const UInputAction* InputAction);

	void UpdateLockOnCamera();
	void OnInteractInput();

	// ===== Bonfire =====
	//게임 시작 시 LastActivatedBonfire가 없으면 맵에서 기본 Bonfire를 탐색해 설정
	void InitializeDefaultBonfire();

	// ===== UI =====
	void InitializeMasterHUD();
	void BindPlayerHUDData();

	void BindDeathStateTagEvent();
	void UnbindDeathStateTagEvent();
	void RefreshDeathScreenVisibilityFromASC();
	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	//InteractionComponent 델리게이트 바인딩/해제
	void BindInteractionPromptEvent();
	void UnbindInteractionPromptEvent();

	//OnInteractableChanged 콜백
	UFUNCTION()
	void OnInteractableChanged(AActor* NewInteractable);

	// ===== Recovering Tag → Interaction Prompt Dimming =====
	void BindRecoveringTagEvent();
	void UnbindRecoveringTagEvent();
	void HandleRecoveringTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TObjectPtr<AActionPracticeCharacter> CachedCharacter = nullptr;

	TWeakObjectPtr<ABonfire> LastActivatedBonfire = nullptr;

	bool bIsSprintPressed = false;
#pragma endregion

#pragma region "Private Functions"

	// ===== GAS Input Binding Wrappers =====
	void OnJumpPressed()          { HandleGASInputPressed(IA_Jump); }
	void OnSprintPressed();		//Triggered이지만 입력을 시작 시 한 번만 받도록
	void OnSprintReleased();     
	void OnCrouchPressed()        { HandleGASInputPressed(IA_Crouch); }
	void OnRollPressed()          { HandleGASInputPressed(IA_Roll); }
	void OnAttackPressed()        { HandleGASInputPressed(IA_Attack); }
	void OnBlockPressed()         { HandleGASInputPressed(IA_Block); }
	void OnBlockReleased()        { HandleGASInputReleased(IA_Block); }
	void OnChargeAttackPressed()  { HandleGASInputPressed(IA_ChargeAttack); }
	void OnChargeAttackReleased()    { HandleGASInputReleased(IA_ChargeAttack); }
	void OnUseItemPressed()          { HandleGASInputPressed(IA_UseItem); }
	void OnSpecialActionPressed()    { HandleGASInputPressed(IA_SpecialAction); }

#pragma endregion
};
