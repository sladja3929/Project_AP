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

	// ===== UI =====
	void InitializeMasterHUD();
	void BindPlayerHUDData();

	void BindDeathStateTagEvent();
	void UnbindDeathStateTagEvent();
	void RefreshDeathScreenVisibilityFromASC();
	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TObjectPtr<AActionPracticeCharacter> CachedCharacter = nullptr;

	TWeakObjectPtr<ABonfire> LastActivatedBonfire = nullptr;

#pragma endregion

#pragma region "Private Functions"

	// ===== GAS Input Binding Wrappers =====
	void OnJumpPressed()          { HandleGASInputPressed(IA_Jump); }
	void OnSprintPressed()        { HandleGASInputPressed(IA_Sprint); }
	void OnSprintReleased()       { HandleGASInputReleased(IA_Sprint); }
	void OnCrouchPressed()        { HandleGASInputPressed(IA_Crouch); }
	void OnRollPressed()          { HandleGASInputPressed(IA_Roll); }
	void OnAttackPressed()        { HandleGASInputPressed(IA_Attack); }
	void OnBlockPressed()         { HandleGASInputPressed(IA_Block); }
	void OnBlockReleased()        { HandleGASInputReleased(IA_Block); }
	void OnChargeAttackPressed()  { HandleGASInputPressed(IA_ChargeAttack); }
	void OnChargeAttackReleased()    { HandleGASInputReleased(IA_ChargeAttack); }
	void OnUseItemPressed()          { HandleGASInputPressed(IA_UseItem); }

#pragma endregion
};
