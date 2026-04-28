#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GAS/AbilitySet/AbilitySetDataAsset.h"
#include "GAS/CharacterStats/CharacterStatsDataAsset.h"
#include "BaseCharacter.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
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

	//===== Replication =====
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	//캐릭터 생성 시 부여할 AbilitySet 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TObjectPtr<UAbilitySetDataAsset>> StartAbilitySetsData;

	//캐릭터 초기 스탯 DA (어트리뷰트 초기값 정의)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UCharacterStatsDataAsset> CharacterStatsData;

	//부여된 AbilitySet 핸들 (회수용)
	TArray<FAbilitySetGrantedHandles> GrantedSetHandles;

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

	//초기 어트리뷰트 적용
	virtual void ApplyInitialAttributes();

	//AbilitySet 부여
	virtual void GrantStartupAbilitySets();

	//부여된 AbilitySet 전부 회수
	virtual void RemoveAllAbilitySets();

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