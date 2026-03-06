#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/UsableItemDataAsset.h"
#include "ItemManagerComponent.generated.h"

class AActionPracticeCharacter;

//아이템 슬롯 변경 시 UI 갱신 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquippedItemChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UItemManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnEquippedItemChanged OnEquippedItemChanged;

	//초기 아이템 구성 (BeginPlay에서 Slots로 복사)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FUsableItemSlot> DefaultSlots;

#pragma endregion

#pragma region "Public Functions"

	UItemManagerComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//현재 장착 슬롯의 DA 반환 (어빌리티에서 사용)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	const UUsableItemDataAsset* GetEquippedItemDA() const;

	//현재 장착 슬롯 정보 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	const FUsableItemSlot& GetEquippedSlot() const;

	//현재 장착 아이템 사용 가능 여부
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool CanUseEquippedItem() const;

	//퀵슬롯 전환 (다음 아이템으로)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CycleQuickSlot();

	//아이템 사용 (수량 차감) — 서버에서만 호출
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeEquippedItem();

#pragma endregion

protected:
#pragma region "Protected Variables"

	//실제 인벤토리 데이터 (서버 권위, 클라이언트 동기화)
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FUsableItemSlot> Slots;

	//현재 장착된 퀵슬롯 인덱스
	UPROPERTY(ReplicatedUsing = OnRep_EquippedIndex)
	int32 EquippedIndex = 0;

	//Owner 캐싱
	UPROPERTY()
	TObjectPtr<AActionPracticeCharacter> OwnerCharacter = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_EquippedIndex();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"

	//퀵슬롯 전환 서버 RPC
	UFUNCTION(Server, Reliable)
	void Server_CycleQuickSlot();

#pragma endregion
};
