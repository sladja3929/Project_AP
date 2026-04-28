     #include "GAS/Abilities/Player/UseItemAbility.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/ItemManagerComponent.h"
#include "Characters/WeaponManagerComponent.h"
#include "Items/Weapon.h"
#include "Items/UsableItemDataAsset.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogUseItemAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogUseItemAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

const FName UUseItemAbility::CurveName_ApplyEffect = TEXT("ApplyEffect");

UUseItemAbility::UUseItemAbility()
{
	//아이템 사용은 재활성화 가능 (다시 사용)
	bRetriggerInstancedAbility = true;

	//아이템 사용은 스태미나 소모 없음
	StaminaCost = -1.0f;

	//회전 불필요 (제자리에서 사용)
	RotateTime = 0.0f;

	//네트워크: LocalPredicted
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UUseItemAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EffectUsableItemMagnitudeTag = UGameplayTagsSubsystem::GetEffectUsableItemMagnitudeTag();
	EffectUsableItemDurationTag = UGameplayTagsSubsystem::GetEffectUsableItemDurationTag();

	if (!EffectUsableItemMagnitudeTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectUsableItemMagnitudeTag is not valid"));
	}
	if (!EffectUsableItemDurationTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectUsableItemDurationTag is not valid"));
	}
}

bool UUseItemAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	//ItemManagerComponent에서 현재 장착 아이템 사용 가능 여부 확인
	const AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo(ActorInfo);
	if (!Character)
	{
		DEBUG_LOG(TEXT("CanActivateAbility: No Character"));
		return false;
	}

	const UItemManagerComponent* ItemManager = Character->GetItemManagerComponent();
	if (!ItemManager)
	{
		DEBUG_LOG(TEXT("CanActivateAbility: No ItemManagerComponent"));
		return false;
	}

	if (!ItemManager->CanUseEquippedItem())
	{
		DEBUG_LOG(TEXT("CanActivateAbility: Cannot use equipped item"));
		return false;
	}

	return true;
}

void UUseItemAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	bEffectApplied = false;
	CachedItemDA = nullptr;
	DestroyItemMesh();

	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character)
	{
		DEBUG_LOG(TEXT("ActivateInitSettings: No Character"));
		return;
	}

	UItemManagerComponent* ItemManager = Character->GetItemManagerComponent();
	if (!ItemManager)
	{
		DEBUG_LOG(TEXT("ActivateInitSettings: No ItemManagerComponent"));
		return;
	}

	CachedItemDA = ItemManager->GetEquippedItemDA();
	if (!CachedItemDA)
	{
		DEBUG_LOG(TEXT("ActivateInitSettings: No equipped item DA"));
	}
}

void UUseItemAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CachedItemDA)
	{
		DEBUG_LOG(TEXT("No CachedItemDA - EndAbility"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//몽타주 프리로드 (TSoftObjectPtr이므로)
	const_cast<UUsableItemDataAsset*>(CachedItemDA.Get())->PreloadMontage();

	SetWeaponsVisibility(false);
	SpawnItemMesh();

	//회전 없이 바로 몽타주 재생
	StartMontageWithEventsTask();
}

bool UUseItemAbility::ConsumeStamina()
{
	//아이템 사용은 스태미나 소모 없음
	return true;
}

UAnimMontage* UUseItemAbility::SetMontageToPlayTask()
{
	if (!CachedItemDA)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No CachedItemDA"));
		return nullptr;
	}

	UAnimMontage* Montage = CachedItemDA->UseMontage.LoadSynchronous();
	if (!Montage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load UseMontage"));
	}

	return Montage;
}

void UUseItemAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("No MontageWithEvents Task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//부모 설정 (몽타주 콜백 + EnableBufferInput/ActionRecovery 커브)
	Super::SetUpPlayMontageWithEventsTask();

	//ApplyEffect 커브 추가 등록
	PlayMontageWithEventsTask->EnableCurvePolling(CurveName_ApplyEffect);
}

void UUseItemAbility::OnCurveRisingEdgeReceived(FName CurveName)
{
	//부모 클래스 커브 처리 (EnableBufferInput, ActionRecovery)
	Super::OnCurveRisingEdgeReceived(CurveName);

	if (CurveName == CurveName_ApplyEffect)
	{
		ApplyItemEffect();
	}
}

void UUseItemAbility::ApplyItemEffect()
{
	//중복 적용 방지
	if (bEffectApplied)
	{
		DEBUG_LOG(TEXT("ApplyItemEffect: Already applied"));
		return;
	}

	if (!CachedItemDA)
	{
		DEBUG_LOG(TEXT("ApplyItemEffect: No CachedItemDA"));
		return;
	}

	if (!CachedItemDA->EffectToApply)
	{
		DEBUG_LOG(TEXT("ApplyItemEffect: No EffectToApply in DA"));
		return;
	}

	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("ApplyItemEffect: No APASC"));
		return;
	}

	//GE Spec 생성
	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle EffectSpec = APASC->CreateGameplayEffectSpec(
		CachedItemDA->EffectToApply, EffectiveLevel, this);

	if (!EffectSpec.IsValid())
	{
		DEBUG_LOG(TEXT("ApplyItemEffect: Failed to create EffectSpec"));
		return;
	}

	//SetByCaller: Magnitude (회복량 등)
	APASC->SetSpecSetByCallerMagnitude(EffectSpec, EffectUsableItemMagnitudeTag, CachedItemDA->EffectMagnitude);

	//SetByCaller: Duration (0보다 크면 Duration GE)
	if (CachedItemDA->EffectDuration > 0.0f)
	{
		APASC->SetSpecSetByCallerMagnitude(EffectSpec, EffectUsableItemDurationTag, CachedItemDA->EffectDuration);
	}

	//GE 적용 (Instant GE는 즉시 제거되므로 반환 핸들이 항상 invalid — 정상 동작)
	APASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	bEffectApplied = true;
	DEBUG_LOG(TEXT("ApplyItemEffect: Effect applied. Magnitude=%.2f Duration=%.2f"),
		CachedItemDA->EffectMagnitude, CachedItemDA->EffectDuration);

	//아이템 수량 차감 (서버 권위)
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (Character)
	{
		UItemManagerComponent* ItemManager = Character->GetItemManagerComponent();
		if (ItemManager)
		{
			ItemManager->ConsumeEquippedItem();
		}
	}
}

void UUseItemAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("UseItem Montage Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UUseItemAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("UseItem Montage Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UUseItemAbility::SpawnItemMesh()
{
	if (!CachedItemDA) return;

	//소켓이 지정되지 않았으면 소품 미표시
	if (CachedItemDA->UseSocketName.IsNone()) return;

	//메시가 없으면 소품 미표시
	if (CachedItemDA->UseMesh.IsNull()) return;

	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!CharacterMesh) return;

	//TSoftObjectPtr 동기 로드
	UStaticMesh* ItemMesh = CachedItemDA->UseMesh.LoadSynchronous();
	if (!ItemMesh)
	{
		DEBUG_LOG(TEXT("SpawnItemMesh: Failed to load UseMesh"));
		return;
	}

	//StaticMeshComponent 생성
	SpawnedItemMeshComponent = NewObject<UStaticMeshComponent>(Character);
	if (!SpawnedItemMeshComponent)
	{
		DEBUG_LOG(TEXT("SpawnItemMesh: Failed to create StaticMeshComponent"));
		return;
	}

	SpawnedItemMeshComponent->SetStaticMesh(ItemMesh);
	SpawnedItemMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//캐릭터 메시의 소켓에 부착
	SpawnedItemMeshComponent->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		CachedItemDA->UseSocketName
	);

	SpawnedItemMeshComponent->SetRelativeLocation(CachedItemDA->UseMeshOffset);
	SpawnedItemMeshComponent->SetRelativeRotation(CachedItemDA->UseMeshRotation);
	SpawnedItemMeshComponent->SetRelativeScale3D(CachedItemDA->UseMeshScale);

	SpawnedItemMeshComponent->RegisterComponent();

	DEBUG_LOG(TEXT("SpawnItemMesh: Attached %s to socket %s"),
		*ItemMesh->GetName(), *CachedItemDA->UseSocketName.ToString());
}

void UUseItemAbility::DestroyItemMesh()
{
	if (SpawnedItemMeshComponent)
	{
		SpawnedItemMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		SpawnedItemMeshComponent->DestroyComponent();
		SpawnedItemMeshComponent = nullptr;

		DEBUG_LOG(TEXT("DestroyItemMesh: Item mesh removed"));
	}
}

void UUseItemAbility::SetWeaponsVisibility(bool bVisible)
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
}

void UUseItemAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	DestroyItemMesh();
	SetWeaponsVisibility(true);

	CachedItemDA = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
