#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "WeaponManagerComponent.generated.h"

class AWeapon;
class AActionPracticeCharacter;
enum class EWeaponEnums : uint8;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UWeaponManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FString WeaponBlueprintBasePath = TEXT("/Game/Items/BluePrint/");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeapon> RightWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeapon> LeftWeaponClass;

#pragma endregion

#pragma region "Public Functions"

	UWeaponManagerComponent();

	virtual void BeginPlay() override;

	FORCEINLINE AWeapon* GetLeftWeapon() const { return LeftWeapon; }
	FORCEINLINE AWeapon* GetRightWeapon() const { return RightWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipWeapon(TSubclassOf<AWeapon> NewWeaponClass, bool bIsLeftHand = true, bool bIsTwoHanded = false);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void UnequipWeapon(bool bIsLeftHand = true);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	TSubclassOf<AWeapon> LoadWeaponClassByName(const FString& WeaponName);

	void WeaponSwitch();

	TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

	TSubclassOf<AWeapon> WeaponClass = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", ReplicatedUsing = OnRep_LeftWeapon, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AWeapon> LeftWeapon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", ReplicatedUsing = OnRep_RightWeapon, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AWeapon> RightWeapon = nullptr;

	bool bIsSwitching = false;

#pragma endregion

#pragma region "Private Functions"

	UFUNCTION()
	void OnRep_LeftWeapon();

	UFUNCTION()
	void OnRep_RightWeapon();

	//무기 타입과 손 방향에 따른 소켓 이름 결정
	FName ResolveSocketName(EWeaponEnums WeaponType, bool bIsLeftHand) const;

#pragma endregion
};
