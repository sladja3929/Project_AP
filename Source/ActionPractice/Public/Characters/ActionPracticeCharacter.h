#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Logging/LogMacros.h"
#include "ActionPracticeCharacter.generated.h"

class UInputActionDataAsset;
class IHitDetectionInterface;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UAbilitySystemComponent;
class UGameplayAbility;
class UInputBufferComponent;
class AWeapon;
class UPlayerStatsWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All); 

UCLASS(abstract)
class AActionPracticeCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FString WeaponBlueprintBasePath = TEXT("/Game/Items/BluePrint/");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeapon> RightWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeapon> LeftWeaponClass;
	
	// ===== Movement Properties =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 400.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeedMultiplier = 1.5f;
    
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float CrouchSpeedMultiplier = 0.5f;

#pragma endregion
	
#pragma region "Public Functions"
	
	AActionPracticeCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ===== Getter =====
	//Camera
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }	
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	//GAS
	FORCEINLINE UActionPracticeAttributeSet* GetAttributeSet() const { return Cast<UActionPracticeAttributeSet>(AttributeSet); }

	//Input
	FORCEINLINE UInputBufferComponent* GetInputBufferComponent() const { return InputBufferComponent; }
	FORCEINLINE const UInputActionDataAsset* GetInputActionData() const { return InputActionData; }
	
	//Weapon
	FORCEINLINE AWeapon* GetLeftWeapon() const { return LeftWeapon; }
	FORCEINLINE AWeapon* GetRightWeapon() const { return RightWeapon; }
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const override;
	// ===================
	
	//Movement Functions
	UFUNCTION(BlueprintPure, Category = "Input")
	FVector2D GetCurrentMovementInput() const;
	
	UFUNCTION(BlueprintCallable, Category = "Character")
	void RotateCharacterToInputDirection(float RotationTime, bool bIgnoreLockOn);

	TArray<FGameplayAbilitySpec*> FindAbilitySpecsWithInputAction(const UInputAction* InputAction);

	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsBlockInputPressed() const;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent = nullptr;

	// ===== UI Properties =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerStatsWidget> PlayerStatsWidgetClass;

	UPROPERTY()
	TObjectPtr<UPlayerStatsWidget> PlayerStatsWidget;

	// ===== Weapon Properties =====
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeapon> WeaponClass = nullptr;
	
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AWeapon> LeftWeapon = nullptr;
    
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AWeapon> RightWeapon = nullptr;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputActionDataAsset> InputActionData = nullptr;

	// ===== Usage Tags =====
	FGameplayTag StateRecoveringTag;
	FGameplayTag StateAbilitySprintingTag;
	FGameplayTag StateAbilityAttackingTag;
	FGameplayTag AbilityAttackTag;
	
	// ===== State Variables =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action State")
	bool bIsLockOn = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action State")
	bool bIsSwitching = false;

	// ===== LockOn =====
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> LockedOnTarget = nullptr;
	
#pragma endregion

#pragma region "Protected Functions"
	
	// ===== Weapon Functions =====
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	TSubclassOf<AWeapon> LoadWeaponClassByName(const FString& WeaponName);
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipWeapon(TSubclassOf<AWeapon> NewWeaponClass, bool bIsLeftHand = true, bool bIsTwoHanded = false);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void UnequipWeapon(bool bIsLeftHand = true);
	
	// ===== GAS Functions =====
	virtual void InitializeAbilitySystem() override;
	virtual void CreateAbilitySystemComponent() override;
	virtual void CreateAttributeSet() override;

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GASInputPressed(const UInputAction* InputAction);
	
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GASInputReleased(const UInputAction* InputAction);
	
	// ===== Input Handler Functions =====
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void ToggleLockOn();
	void WeaponSwitch();

	// ===== Input Handler Additional Functions =====
	void CancelActionForMove();
	AActor* FindNearestTarget();
	void UpdateLockOnCamera();
	
	// ===== GAS Input Handler Functions =====
	void OnJumpInput();
	void OnSprintInput();
	void OnSprintInputReleased();
	void OnCrouchInput();
	void OnRollInput();
	void OnAttackInput();
	void OnBlockInput();
	void OnBlockInputReleased();
	void OnChargeAttackInput();
	void OnChargeAttackReleased();
	
#pragma endregion

private:
#pragma region "Private Variables"


#pragma endregion

#pragma region "Private Functions"


#pragma endregion
};
