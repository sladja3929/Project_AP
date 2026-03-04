#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Characters/WeaponManagerComponent.h"
#include "Logging/LogMacros.h"
#include "ActionPracticeCharacter.generated.h"

class UActionPracticeAbilitySystemComponent;
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
class ULockOnComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class AActionPracticeCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

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
	FORCEINLINE AWeapon* GetLeftWeapon() const { return WeaponManagerComponent ? WeaponManagerComponent->GetLeftWeapon() : nullptr; }
	FORCEINLINE AWeapon* GetRightWeapon() const { return WeaponManagerComponent ? WeaponManagerComponent->GetRightWeapon() : nullptr; }
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const override;

	//WeaponManagerComponent Getter
	FORCEINLINE UWeaponManagerComponent* GetWeaponManagerComponent() const { return WeaponManagerComponent; }
	// ===================

	//Movement Functions
	UFUNCTION(BlueprintPure, Category = "Input")
	FVector2D GetCurrentMovementInput() const;

	UFUNCTION(BlueprintCallable, Category = "Character")
	void RotateCharacterToInputDirection(float RotationTime, bool bIgnoreLockOn);

	TArray<FGameplayAbilitySpec*> FindAbilitySpecsWithInputAction(const UInputAction* InputAction);

	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsBlockInputPressed() const;

	// ===== Controller-facing execution functions =====
	void ExecuteMove(const FVector2D& MovementVector);
	void ExecuteLook(const FVector2D& LookAxisVector);
	void WeaponSwitch();

	//LockOnComponent Getter
	FORCEINLINE ULockOnComponent* GetLockOnComponent() const { return LockOnComponent; }

	// ===== GAS Input (called by Controller) =====
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GASInputPressed(const UInputAction* InputAction);

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GASInputReleased(const UInputAction* InputAction);

	void TryAutoActivateAttackSequenceAbility();

	//===== Replication =====
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULockOnComponent> LockOnComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponManagerComponent> WeaponManagerComponent = nullptr;

	// ===== UI Properties =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerStatsWidget> PlayerStatsWidgetClass;

	UPROPERTY()
	TObjectPtr<UPlayerStatsWidget> PlayerStatsWidget;

	// ===== Input Action Data (for GAS ability mapping) =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputActionDataAsset> InputActionData = nullptr;

	// ===== Usage Tags =====
	FGameplayTag StateRecoveringLocalTag;
	FGameplayTag StateAbilitySprintingTag;
	FGameplayTag StateAbilityRollingTag;
	FGameplayTag StateAbilityAttackingLocalTag;
	FGameplayTag AbilityAttackTag;
	FGameplayTag EventActionAttackInputTag;
	FGameplayTag EventActionCancelAttackTag;

	// AttackSequenceAbility auto-activation gate
	bool bAttackSequenceAutoActivated = false;
	int32 AttackSequenceAutoActivateRetryCount = 0;
	FTimerHandle AttackSequenceAutoActivateTimer;

#pragma endregion

#pragma region "Protected Functions"

	//현재 상태 태그를 캡처하여 반환
	FGameplayTagContainer CaptureCurrentStateTags() const;

	// ===== GAS Functions =====
	virtual void InitializeAbilitySystem() override;
	virtual void CreateAbilitySystemComponent() override;
	virtual void CreateAttributeSet() override;

	bool IsAttackSequenceAutoActivateReady() const;

	// ===== Input Handler Additional Functions =====
	void CancelActionForMove();

	// ===== Rotation Helper =====
	bool CalculateYawFromMovementInput(float& OutYaw) const;

	//서버에 회전 요청(RPC)
	UFUNCTION(Server, Reliable)
	void Server_RequestRotateToYaw(float TargetYaw, float RotateTime);


#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TObjectPtr<UActionPracticeAbilitySystemComponent> APASC = nullptr;

#pragma endregion

#pragma region "Private Functions"


#pragma endregion
};
