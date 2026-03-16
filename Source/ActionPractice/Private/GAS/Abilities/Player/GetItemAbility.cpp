#include "GAS/Abilities/Player/GetItemAbility.h"
#include "Games/ActionPracticePlayerController.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/ItemManagerComponent.h"
#include "Characters/InteractionComponent.h"
#include "Characters/WeaponManagerComponent.h"
#include "Items/Weapon.h"
#include "Interaction/PickupItem.h"
#include "Items/BaseItemDataAsset.h"
#include "Items/UsableItemDataAsset.h"
#include "AbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogGetItemAbility, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogGetItemAbility, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UGetItemAbility::UGetItemAbility()
{
	//Bonfire의 Interact에서 서버가 TryActivateAbilityWithEventData 호출 → ServerInitiated
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGetItemAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//PickupItem 참조 획득
	AcquirePickupItem(TriggerEventData);

	if (!CachedPickupItem.IsValid())
	{
		DEBUG_LOG(TEXT("ActivateAbility: No PickupItem reference — aborting"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//아이템 즉시 획득 (몽타주 재생 전)
	const bool bSuccess = ProcessItemAcquisition();
	if (!bSuccess)
	{
		DEBUG_LOG(TEXT("ActivateAbility: Item acquisition failed — aborting"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//PickupItem 비활성화 (서버)
	CachedPickupItem->OnPickedUp();

	//클라이언트에 획득 알림 전달
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (Character)
	{
		AActionPracticePlayerController* PC = Cast<AActionPracticePlayerController>(Character->GetController());
		if (PC)
		{
			PC->Client_NotifyItemAcquired(
				const_cast<UBaseItemDataAsset*>(CachedPickupItem->GetItemDA()),
				CachedPickupItem->GetItemCount()
			);
		}
	}

	//몽타주 재생 전 무기 숨김
	SetWeaponsVisibility(false);

	//픽업 몽타주 재생 — 순수 연출, 캔슬되어도 아이템은 이미 인벤에 있음
	StartMontageWithEventsTask();
}

void UGetItemAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CachedPickupItem = nullptr;
	SetWeaponsVisibility(true);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#pragma region "Montage Settings"

UAnimMontage* UGetItemAbility::SetMontageToPlayTask()
{
	if (!PickupMontage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No PickupMontage assigned"));
	}
	return PickupMontage;
}

void UGetItemAbility::SetUpPlayMontageWithEventsTask()
{
	//현재는 몽타주 이벤트 콜백 추가 연결 없음 — 확장 시 여기에 추가
}

void UGetItemAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("StartMontageWithEventsTask: No montage — ending"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		1.0f
	);

	if (PlayMontageWithEventsTask)
	{
		SetUpPlayMontageWithEventsTask();

		PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &UGetItemAbility::OnTaskMontageCompleted);
		PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &UGetItemAbility::OnTaskMontageInterrupted);
		PlayMontageWithEventsTask->ReadyForActivation();

		DEBUG_LOG(TEXT("StartMontageWithEventsTask: Montage started — %s"), *MontageToPlay->GetName());
	}
	else
	{
		DEBUG_LOG(TEXT("StartMontageWithEventsTask: Task creation failed — ending"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGetItemAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("GetItem Montage Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGetItemAbility::OnTaskMontageInterrupted()
{
	//몽타주가 캔슬되어도 아이템은 이미 획득됨 — 정상 종료
	DEBUG_LOG(TEXT("GetItem Montage Interrupted — item already acquired"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

#pragma endregion

#pragma region "Helpers"

void UGetItemAbility::SetWeaponsVisibility(bool bVisible)
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	UWeaponManagerComponent* WeaponManager = Character->GetWeaponManagerComponent();
	if (!WeaponManager) return;

	if (AWeapon* LeftWeapon = WeaponManager->GetLeftWeapon())
	{
		LeftWeapon->SetActorHiddenInGame(!bVisible);
	}

	if (AWeapon* RightWeapon = WeaponManager->GetRightWeapon())
	{
		RightWeapon->SetActorHiddenInGame(!bVisible);
	}

	DEBUG_LOG(TEXT("SetWeaponsVisibility: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UGetItemAbility::AcquirePickupItem(const FGameplayEventData* TriggerEventData)
{
	//1순위: TriggerEventData->OptionalObject (PickupItem::Interact 경로)
	if (TriggerEventData && TriggerEventData->OptionalObject)
	{
		CachedPickupItem = Cast<APickupItem>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));
		if (CachedPickupItem.IsValid())
		{
			DEBUG_LOG(TEXT("AcquirePickupItem: From TriggerEventData.OptionalObject"));
			return;
		}
	}

	//2순위: InteractionComponent 캐시
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (Character)
	{
		if (UInteractionComponent* InteractionComp = Character->GetInteractionComponent())
		{
			CachedPickupItem = Cast<APickupItem>(InteractionComp->GetCurrentInteractable());
			if (CachedPickupItem.IsValid())
			{
				DEBUG_LOG(TEXT("AcquirePickupItem: From InteractionComponent cache"));
				return;
			}
		}
	}

	DEBUG_LOG(TEXT("AcquirePickupItem: PickupItem reference not found"));
}

bool UGetItemAbility::ProcessItemAcquisition()
{
	if (!CachedPickupItem.IsValid()) return false;

	UBaseItemDataAsset* DA = CachedPickupItem->GetItemDA();
	const int32 Count = CachedPickupItem->GetItemCount();
	if (!DA)
	{
		DEBUG_LOG(TEXT("ProcessItemAcquisition: No ItemDA on PickupItem"));
		return false;
	}

	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return false;

	//UsableItemDataAsset 처리 — 현재 유일한 분기이지만, 확장 가능하도록 분리
	if (UUsableItemDataAsset* UsableDA = Cast<UUsableItemDataAsset>(DA))
	{
		UItemManagerComponent* ItemManager = Character->GetItemManagerComponent();
		if (!ItemManager)
		{
			DEBUG_LOG(TEXT("ProcessItemAcquisition: No ItemManagerComponent"));
			return false;
		}

		const bool bAdded = ItemManager->AddUsableItem(UsableDA, Count);
		DEBUG_LOG(TEXT("ProcessItemAcquisition: UsableItem %s — %s"), *DA->DisplayName.ToString(), bAdded ? TEXT("Success") : TEXT("Failed"));
		return bAdded;
	}

	//향후 다른 아이템 타입 분기 추가 지점
	//ex) if (UWeaponItemDataAsset* WeaponDA = Cast<UWeaponItemDataAsset>(DA)) { ... }

	DEBUG_LOG(TEXT("ProcessItemAcquisition: Unsupported item type — %s"), *DA->GetClass()->GetName());
	return false;
}

#pragma endregion
