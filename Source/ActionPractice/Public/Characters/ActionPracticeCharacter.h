#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
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

	// ===== Controller-facing execution functions =====
	void ExecuteMove(const FVector2D& MovementVector);
	void ExecuteLook(const FVector2D& LookAxisVector);
	void WeaponSwitch();

	// ===== Lock-On Interface (set by Controller) =====
	void SetLockedOnTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsLockedOn() const { return bIsLockOn; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor* GetLockOnTarget() const { return LockedOnTarget; }

	// ===== GAS Input (called by Controller) =====
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GASInputPressed(const UInputAction* InputAction);

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GASInputReleased(const UInputAction* InputAction);

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

	// ===== UI Properties =====
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerStatsWidget> PlayerStatsWidgetClass;

	UPROPERTY()
	TObjectPtr<UPlayerStatsWidget> PlayerStatsWidget;

	// ===== Weapon Properties =====
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeapon> WeaponClass = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", ReplicatedUsing = OnRep_LeftWeapon)
	TObjectPtr<AWeapon> LeftWeapon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", ReplicatedUsing = OnRep_RightWeapon)
	TObjectPtr<AWeapon> RightWeapon = nullptr;

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

	// ===== State Variables =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action State")
	bool bIsSwitching = false;

	// AttackSequenceAbility auto-activation gate
	bool bAttackSequenceAutoActivated = false;
	int32 AttackSequenceAutoActivateRetryCount = 0;
	FTimerHandle AttackSequenceAutoActivateTimer;

#pragma endregion

#pragma region "Protected Functions"

	//현재 상태 태그를 캡처하여 반환
	FGameplayTagContainer CaptureCurrentStateTags() const;

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

	void TryAutoActivateAttackSequenceAbility();
	bool IsAttackSequenceAutoActivateReady() const;

	// ===== Input Handler Additional Functions =====
	void CancelActionForMove();

	// ===== Rotation Helper =====
	bool CalculateYawFromMovementInput(float& OutYaw) const;

	//서버에 회전 요청(RPC)
	UFUNCTION(Server, Reliable)
	void Server_RequestRotateToYaw(float TargetYaw, float RotateTime);

	//락온 상태를 서버에 동기화
	UFUNCTION(Server, Reliable)
	void ServerSetLockOnState(bool bNewLockOn, AActor* NewTarget);

	//CMC 회전 모드를 서버에 동기화
	UFUNCTION(Server, Reliable)
	void ServerSetRotationMode(bool bOrientToMovement, bool bUseControllerDesired);

	//로컬 CMC 세팅 + 서버 동기화를 하나로 묶는 헬퍼
	void SetRotationMode(bool bOrientToMovement, bool bUseControllerDesired);

	//현재 회전 모드 캐싱 (불필요한 RPC 방지)
	bool bCachedOrientToMovement = true;
	bool bCachedUseControllerDesired = false;

	//===== Replication Functions =====
	UFUNCTION()
	void OnRep_LeftWeapon();

	UFUNCTION()
	void OnRep_RightWeapon();

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TObjectPtr<UActionPracticeAbilitySystemComponent> APASC = nullptr;

	// ===== Lock-On State (replicated, set via SetLockedOnTarget) =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action State", Replicated, meta = (AllowPrivateAccess = "true"))
	bool bIsLockOn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat", Replicated, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> LockedOnTarget = nullptr;

#pragma endregion

#pragma region "Private Functions"


#pragma endregion
};
