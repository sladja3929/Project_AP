#include "Characters/ItemManagerComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 1

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
	OnEquippedItemChanged.Broadcast();
	return true;
}

void UItemManagerComponent::OnRep_Slots()
{
	DEBUG_LOG(TEXT("OnRep_Slots: %d slots replicated"), Slots.Num());
	OnEquippedItemChanged.Broadcast();
}

void UItemManagerComponent::OnRep_EquippedIndex()
{
	DEBUG_LOG(TEXT("OnRep_EquippedIndex: %d"), EquippedIndex);
	OnEquippedItemChanged.Broadcast();
}
