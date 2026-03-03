#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ActionPracticePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class AActionPracticeCharacter;

/**
 *  PlayerController for ActionPractice
 *  Manages input mappings, input bindings, lock-on system
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
	TObjectPtr<UInputAction> IA_WeaponSwitch = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_ChargeAttack = nullptr;

	// ===== Lock-On State =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	bool bIsLockOn = false;

	UPROPERTY(BlueprintReadOnly, Category="Combat")
	TObjectPtr<AActor> LockedOnTarget = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	/** Input mapping context + action binding setup */
	virtual void SetupInputComponent() override;

	// ===== Input Handlers =====
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleToggleLockOn();
	void HandleWeaponSwitch();
	void HandleGASInputPressed(const UInputAction* InputAction);
	void HandleGASInputReleased(const UInputAction* InputAction);

	// ===== Lock-On Helpers =====
	AActor* FindNearestTarget();
	void UpdateLockOnCamera();

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TObjectPtr<AActionPracticeCharacter> CachedCharacter = nullptr;

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
	void OnChargeAttackReleased() { HandleGASInputReleased(IA_ChargeAttack); }

#pragma endregion
};
