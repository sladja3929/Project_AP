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

	//UsableItem을 슬롯에 추가 — 서버에서만 호출
	//동일 DA 슬롯이 이미 있으면 수량 추가, 없으면 새 슬롯 생성
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddUsableItem(const UUsableItemDataAsset* InItemDA, int32 InCount);

	//Refillable 아이템 전체 리필 — 서버에서만 호출 (휴식 시 사용)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefillAllSlots();

	//EquippedIndex 기준 상대 오프셋 슬롯 반환 (0=현재, 1=다음, 2=다다음...)
	//Slots가 비거나 범위 밖이면 빈 슬롯 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	const FUsableItemSlot& GetSlotAtOffset(int32 Offset) const;

	//전체 슬롯 수 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetSlotCount() const;

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

	//현재 장착 슬롯 아이템의 몽타주/메시를 프리로드
	//슬롯 초기화(BeginPlay)/전환(CycleQuickSlot)/복제(OnRep) 시점에 호출해
	//사용 시점의 동기 로드 hitch를 앞당긴다 (서버/클라 모두)
	void PreloadEquippedItemAssets();

#pragma endregion
};
