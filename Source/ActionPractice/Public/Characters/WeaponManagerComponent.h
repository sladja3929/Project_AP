#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "WeaponManagerComponent.generated.h"

class AWeapon;
class AActionPracticeCharacter;
class UAnimInstance;
enum class EWeaponEnums : uint8;

//무기 변경 시 UI 갱신 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, bool, bIsLeftHand);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UWeaponManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponChanged OnWeaponChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FString WeaponBlueprintBasePath = TEXT("/Game/Items/BluePrint/");

	//에디터에서 설정할 오른손 무기 목록 (첫 번째가 시작 무기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TArray<TSubclassOf<AWeapon>> DefaultRightWeapons;

	//에디터에서 설정할 왼손 무기 목록 (첫 번째가 시작 무기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TArray<TSubclassOf<AWeapon>> DefaultLeftWeapons;

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

	//오른손 무기 순환
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CycleRightWeapon();

	//왼손 무기 순환
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CycleLeftWeapon();

	TScriptInterface<IHitDetectionInterface> GetHitDetectionInterface() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

	//무기 변경 시 해당 팔의 애니메이션 레이어 ABP 교체
	void UpdateAnimationLayer(AWeapon* NewWeapon, bool bIsLeftHand);

	//특정 팔의 Link된 레이어 ABP를 Unlink
	void UnlinkAnimationLayer(bool bIsLeftHand);

#pragma endregion

private:
#pragma region "Private Variables"

	TSubclassOf<AWeapon> WeaponClass = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", ReplicatedUsing = OnRep_LeftWeapon, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AWeapon> LeftWeapon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", ReplicatedUsing = OnRep_RightWeapon, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AWeapon> RightWeapon = nullptr;

	//현재 장착된 무기 인덱스
	int32 RightWeaponIndex = 0;
	int32 LeftWeaponIndex = 0;

	//현재 Link된 좌팔 레이어 ABP 클래스 (Unlink용 캐싱)
	TSubclassOf<UAnimInstance> CurrentLeftArmLayerClass = nullptr;

	//현재 Link된 우팔 레이어 ABP 클래스 (Unlink용 캐싱)
	TSubclassOf<UAnimInstance> CurrentRightArmLayerClass = nullptr;

	//디버그 트레이스 표시 상태 (무기 스위치에도 유지)
	bool bWeaponDebugTrace = false;

	//히트디텍션 전략 토글 상태의 단일 소스 (true=AttackTrace, false=CapsuleOverlap)
	bool bUseTraceDetection = true;

#pragma endregion

#pragma region "Private Functions"

	UFUNCTION()
	void OnRep_LeftWeapon();

	UFUNCTION()
	void OnRep_RightWeapon();

	//무기 타입과 손 방향에 따른 소켓 이름 결정
	FName ResolveSocketName(EWeaponEnums WeaponType, bool bIsLeftHand) const;

	//"1" 키로 디버그 트레이스 on/off
	void ToggleWeaponDebugTrace();

	//현재 장착된 무기 WeaponAttackComponent들에 디버그 상태 주입
	void ApplyDebugTraceToCurrentWeapons();

	//"3" 키로 히트디텍션 전략(AttackTrace <-> CapsuleOverlap) 토글 - 오른손 무기 재장착 방식
	void ToggleHitDetectionStrategy();

#pragma endregion
};
