#pragma once

#include "Public/Items/WeaponEnums.h"
#include "Public/Items/AttackData.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Weapon.generated.h"

struct FOnAttributeChangeData;
class UWeaponCCDComponent;
class AActionPracticeCharacter;
class UWeaponDataAsset;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UWeaponAttackComponent;  
struct FGameplayTag;
struct FBlockActionData;
struct FTaggedAttackData;

UCLASS()
class AWeapon : public AActor
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//HitDetection에 WeaponTraceComponent를 사용할지, WeaponCCDComponent를 사용할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bIsTraceDetectionOrNot = true;
	
#pragma endregion

#pragma region "Public Functions"
	AWeapon();
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// ===== Getter =====
	FORCEINLINE FString GetWeaponName() const {return WeaponName;}
	EWeaponEnums GetWeaponType() const;

	FORCEINLINE AActionPracticeCharacter* GetOwnerCharacter() const {return OwnerCharacter;}
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	const UWeaponDataAsset* GetWeaponData() const { return WeaponData.Get(); }
	
	const FBlockActionData* GetWeaponBlockData() const;
	const FTaggedAttackData* GetWeaponAttackDataByTag(const FGameplayTagContainer& AttackTags) const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	TScriptInterface<IHitDetectionInterface> GetHitDetectionComponent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	FORCEINLINE float GetCalculatedDamage() const {return CalculatedDamage;}
	// ==================

	void CalculateCalculatedDamage();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void EquipWeapon();	

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
#pragma endregion

protected:
#pragma region "Protected Variables"
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponAttackComponent> AttackTraceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponCCDComponent> CCDComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
	FString WeaponName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stats")
	TObjectPtr<UWeaponDataAsset> WeaponData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stats")
	float CalculatedDamage;

	//플레이어 근력/기량 Attribute가 바뀔 때
	FDelegateHandle PlayerStrengthChangedHandle;
	FDelegateHandle PlayerDexterityChangedHandle;

	//WeaponHit 델리게이트 핸들
	FDelegateHandle AttackTraceHitHandle;
	FDelegateHandle CCDHitHandle;
	
#pragma endregion

#pragma region "Protected Functions"

	UFUNCTION()
	void HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData FinalAttackData);

	void OnStrengthChanged(const FOnAttributeChangeData& Data);
	void OnDexterityChanged(const FOnAttributeChangeData& Data);

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY()
	TObjectPtr<AActionPracticeCharacter> OwnerCharacter;
	
#pragma endregion

#pragma region "Private Functions"

	void BindDelegates();
	void UnbindDelegates();

#pragma endregion
};