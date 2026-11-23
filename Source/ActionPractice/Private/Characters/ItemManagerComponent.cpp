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

bool UItemManagerComponent::ConsumeItem(const UUsableItemDataAsset* InItemDA)
{
	//서버에서만 수량 차감
	if (!GetOwner()->HasAuthority())
	{
		DEBUG_LOG(TEXT("ConsumeItem called on client - ignored"));
		return false;
	}

	const int32 SlotIndex = FindSlotIndexByItemDA(InItemDA);

	if (SlotIndex == INDEX_NONE)
	{
		DEBUG_LOG(TEXT("ConsumeItem: No slot for %s"), *GetNameSafe(InItemDA));
		return false;
	}

	FUsableItemSlot& Slot = Slots[SlotIndex];

	if (!Slot.ConsumeOne())
	{
		DEBUG_LOG(TEXT("ConsumeItem: Cannot consume - Count=%d"), Slot.CurrentCount);
		return false;
	}

	DEBUG_LOG(TEXT("ConsumeItem: Consumed %s at index %d. Remaining=%d"), *GetNameSafe(InItemDA), SlotIndex, Slot.CurrentCount);

	//Consumable이고 수량 0이면 슬롯 제거
	if (Slot.ItemDA && Slot.ItemDA->IsConsumable() && Slot.CurrentCount <= 0)
	{
		DEBUG_LOG(TEXT("ConsumeItem: Consumable depleted — removing slot at index %d"), SlotIndex);
		Slots.RemoveAt(SlotIndex);

		//EquippedIndex 보정
		if (Slots.Num() == 0)
		{
			EquippedIndex = 0;
		}
		else if (SlotIndex < EquippedIndex)
		{
			//앞쪽 슬롯 제거 — 장착 중인 아이템 유지
			--EquippedIndex;
		}
		else if (EquippedIndex >= Slots.Num())
		{
			EquippedIndex = 0;
		}
		//장착 슬롯 자체가 제거되면 같은 인덱스의 다음 아이템이 장착됨

		//장착 아이템이 바뀌었을 수 있으므로 프리로드 (클라이언트는 OnRep 시점에 수행)
		PreloadEquippedItemAssets();
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

int32 UItemManagerComponent::FindSlotIndexByItemDA(const UUsableItemDataAsset* InItemDA) const
{
	if (!InItemDA) return INDEX_NONE;

	//같은 DA 슬롯이 여러 개면 장착 슬롯 우선
	if (Slots.IsValidIndex(EquippedIndex) && Slots[EquippedIndex].ItemDA == InItemDA)
	{
		return EquippedIndex;
	}

	//그 외에는 사용 가능한(수량이 남은) 슬롯 우선, 없으면 첫 일치 슬롯
	const int32 UsableIndex = Slots.IndexOfByPredicate([InItemDA](const FUsableItemSlot& Slot)
	{
		return Slot.ItemDA == InItemDA && Slot.CanUse();
	});

	if (UsableIndex != INDEX_NONE)
	{
		return UsableIndex;
	}

	return Slots.IndexOfByPredicate([InItemDA](const FUsableItemSlot& Slot)
	{
		return Slot.ItemDA == InItemDA;
	});
}
