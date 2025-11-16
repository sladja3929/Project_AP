#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "BaseCharacter.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayAbility;
class AWeapon;
class IHitDetectionInterface;
struct FGameplayTag;

UCLASS(abstract)
class ACTIONPRACTICE_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"


#pragma endregion

#pragma region "Public Functions"

	ABaseCharacter();

	virtual void Tick(float DeltaTime) override;

	//===== GAS Interface =====
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//===== Hit Detection Interface =====
	virtual TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const PURE_VIRTUAL(ABaseCharacter::GetHitDetectionInterface, return nullptr;);

#pragma endregion

protected:
#pragma region "Protected Variables"

	//===== GAS Components =====
	//자식 클래스에서 실제 구현
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	//자식 클래스에서 실제 구현
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAttributeSet> AttributeSet = nullptr;

	//캐릭터 생성 시 부여할 기본 어빌리티들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartAbilities;

	//캐릭터 생성 시 적용할 기본 이펙트들
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> StartEffects;

	//===== Rotation Variables =====
	FRotator TargetActionRotation;
	FRotator StartActionRotation;
	float CurrentRotationTime = 0;
	float TotalRotationTime = 0;
	bool bIsRotatingForAction = false;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;

	//===== GAS =====
	//자식 생성자에서 호출
	virtual void CreateAbilitySystemComponent() PURE_VIRTUAL(ABaseCharacter::CreateAttributeSet,);
	virtual void CreateAttributeSet() PURE_VIRTUAL(ABaseCharacter::CreateAttributeSet,);

	virtual void InitializeAbilitySystem();

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void GiveAbility(TSubclassOf<UGameplayAbility> AbilityClass);

	virtual void GrantStartupAbilities();
	virtual void ApplyStartupEffects();

	//===== Rotation Functions =====
	void RotateToRotation(const FRotator& TargetRotation, float RotateTime);
	void RotateToPosition(const FVector& TargetLocation, float RotateTime);

#pragma endregion

private:
#pragma region "Private Variables"


#pragma endregion

#pragma region "Private Functions"

	void UpdateActionRotation(float DeltaTime);

#pragma endregion
};