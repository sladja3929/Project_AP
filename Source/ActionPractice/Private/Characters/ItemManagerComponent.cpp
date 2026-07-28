#include "Characters/ItemManagerComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogItemManagerComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogItemManagerComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UItemManagerComponent::UItemManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UItemManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AActionPracticeCharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		DEBUG_LOG(TEXT("No OwnerCharacter"));
		return;
	}

	//서버에서만 DefaultSlots → Slots 초기화
	if (GetOwner()->HasAuthority())
	{
		Slots = DefaultSlots;
		DEBUG_LOG(TEXT("Inventory initialized with %d slots"), Slots.Num());

		//장착 슬롯 프리로드 (클라이언트는 OnRep_Slots 시점에 수행)
		PreloadEquippedItemAssets();
	}
}

void UItemManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UItemManagerComponent, Slots);
	DOREPLIFETIME(UItemManagerComponent, EquippedIndex);
}

const UUsableItemDataAsset* UItemManagerComponent::GetEquippedItemDA() const
{
	if (!Slots.IsValidIndex(EquippedIndex)) return nullptr;
	return Slots[EquippedIndex].ItemDA;
}

const FUsableItemSlot& UItemManagerComponent::GetEquippedSlot() const
{
	//유효하지 않은 인덱스 대비 static 빈 슬롯
	static const FUsableItemSlot EmptySlot;
	if (!Slots.IsValidIndex(EquippedIndex)) return EmptySlot;
	return Slots[EquippedIndex];
}

bool UItemManagerComponent::CanUseEquippedItem() const
{
	if (!Slots.IsValidIndex(EquippedIndex))
	{
		DEBUG_LOG(TEXT("CanUseEquippedItem: No Valid Index: %d"), EquippedIndex);
		return false;
	}
	return Slots[EquippedIndex].CanUse();
}

void UItemManagerComponent::CycleQuickSlot()
{
	if (Slots.Num() <= 1) return;

	//클라이언트에서 호출 시 서버로 RPC
	if (!GetOwner()->HasAuthority())
	{
		Server_CycleQuickSlot();
		return;
	}

	//서버에서 인덱스 변경
	EquippedIndex = (EquippedIndex + 1) % Slots.Num();

	//전환된 슬롯 아이템 프리로드 (클라이언트는 OnRep_EquippedIndex 시점에 수행)
	PreloadEquippedItemAssets();

	OnEquippedItemChanged.Broadcast();
	DEBUG_LOG(TEXT("QuickSlot cycled to index %d"), EquippedIndex);
}

void UItemManagerComponent::Server_CycleQuickSlot_Implementation()
{
	CycleQuickSlot();
}

bool UItemManagerComponent::ConsumeEquippedItem()
{
	//서버에서만 수량 차감
	if (!GetOwner()->HasAuthority())
	{
		DEBUG_LOG(TEXT("ConsumeEquippedItem called on client - ignored"));
		return false;
	}

	if (!Slots.IsValidIndex(EquippedIndex))
	{
		DEBUG_LOG(TEXT("ConsumeEquippedItem: Invalid EquippedIndex %d"), EquippedIndex);
		return false;
	}

	FUsableItemSlot& Slot = Slots[EquippedIndex];
	if (!Slot.ConsumeOne())
	{
		DEBUG_LOG(TEXT("ConsumeEquippedItem: Cannot consume - Count=%d"), Slot.CurrentCount);
		return false;
	}

	DEBUG_LOG(TEXT("ConsumeEquippedItem: Consumed. Remaining=%d"), Slot.CurrentCount);

	//Consumable이고 수량 0이면 슬롯 제거
	if (Slot.ItemDA && Slot.ItemDA->IsConsumable() && Slot.CurrentCount <= 0)
	{
		DEBUG_LOG(TEXT("ConsumeEquippedItem: Consumable depleted — removing slot at index %d"), EquippedIndex);
		Slots.RemoveAt(EquippedIndex);

		//EquippedIndex 보정
		if (Slots.Num() == 0)
		{
			EquippedIndex = 0;
		}
		else if (EquippedIndex >= Slots.Num())
		{
			EquippedIndex = 0;
		}
		//같은 인덱스면 다음 아이템이 자동으로 장착됨 — 추가 보정 불필요
	}

	OnEquippedItemChanged.Broadcast();
	return true;
}

void UItemManagerComponent::RefillAllSlots()
{
	if (!GetOwner()->HasAuthority())
	{
		DEBUG_LOG(TEXT("RefillAllSlots: Called on client — ignored"));
		return;
	}

	bool bAnyRefilled = false;
	for (FUsableItemSlot& Slot : Slots)
	{
		if (Slot.ItemDA && Slot.ItemDA->IsRefillable())
		{
			const int32 PreviousCount = Slot.CurrentCount;
			Slot.Refill();
			if (PreviousCount != Slot.CurrentCount)
			{
				bAnyRefilled = true;
				DEBUG_LOG(TEXT("RefillAllSlots: %s refilled %d → %d"), *Slot.ItemDA->DisplayName.ToString(), PreviousCount, Slot.CurrentCount);
			}
		}
	}

	if (bAnyRefilled)
	{
		OnEquippedItemChanged.Broadcast();
	}
}

bool UItemManagerComponent::AddUsableItem(const UUsableItemDataAsset* InItemDA, int32 InCount)
{
	if (!GetOwner()->HasAuthority())
	{
		DEBUG_LOG(TEXT("AddUsableItem: Called on client — ignored"));
		return false;
	}

	if (!InItemDA || InCount <= 0)
	{
		DEBUG_LOG(TEXT("AddUsableItem: Invalid params — DA=%s, Count=%d"), *GetNameSafe(InItemDA), InCount);
		return false;
	}

	//기존 슬롯에서 동일 DA 탐색
	for (FUsableItemSlot& Slot : Slots)
	{
		if (Slot.ItemDA == InItemDA)
		{
			//무제한 아이템이면 수량 변경 불필요
			if (InItemDA->IsTool())
			{
				DEBUG_LOG(TEXT("AddUsableItem: %s is unlimited — no count change"), *InItemDA->DisplayName.ToString());
				return true;
			}

			const int32 PreviousCount = Slot.CurrentCount;
			Slot.CurrentCount = FMath::Min(Slot.CurrentCount + InCount, InItemDA->MaxStackCount);
			OnEquippedItemChanged.Broadcast();
			DEBUG_LOG(TEXT("AddUsableItem: %s count %d → %d"), *InItemDA->DisplayName.ToString(), PreviousCount, Slot.CurrentCount);
			return true;
		}
	}

	//새 슬롯 생성
	FUsableItemSlot NewSlot;
	NewSlot.ItemDA = const_cast<UUsableItemDataAsset*>(InItemDA);
	NewSlot.CurrentCount = InItemDA->IsTool() ? 0 : FMath::Min(InCount, InItemDA->MaxStackCount);
	Slots.Add(NewSlot);
	OnEquippedItemChanged.Broadcast();
	DEBUG_LOG(TEXT("AddUsableItem: New slot created — %s, Count=%d"), *InItemDA->DisplayName.ToString(), NewSlot.CurrentCount);
	return true;
}

const FUsableItemSlot& UItemManagerComponent::GetSlotAtOffset(int32 Offset) const
{
	static const FUsableItemSlot EmptySlot;
	if (Slots.Num() == 0) return EmptySlot;

	const int32 TargetIndex = (EquippedIndex + Offset) % Slots.Num();
	if (!Slots.IsValidIndex(TargetIndex)) return EmptySlot;
	return Slots[TargetIndex];
}

int32 UItemManagerComponent::GetSlotCount() const
{
	return Slots.Num();
}

void UItemManagerComponent::OnRep_Slots()
{
	DEBUG_LOG(TEXT("OnRep_Slots: %d slots replicated"), Slots.Num());

	//클라이언트 측 장착 슬롯 프리로드
	PreloadEquippedItemAssets();

	OnEquippedItemChanged.Broadcast();
}

void UItemManagerComponent::OnRep_EquippedIndex()
{
	DEBUG_LOG(TEXT("OnRep_EquippedIndex: %d"), EquippedIndex);

	//클라이언트 측 전환된 슬롯 프리로드
	PreloadEquippedItemAssets();

	OnEquippedItemChanged.Broadcast();
}

void UItemManagerComponent::PreloadEquippedItemAssets()
{
	const UUsableItemDataAsset* ItemDA = GetEquippedItemDA();
	if (!ItemDA) return;

	//PreloadAssets는 non-const지만 DA 상태(캐시)만 채우므로 const_cast 사용
	const_cast<UUsableItemDataAsset*>(ItemDA)->PreloadAssets();
}
