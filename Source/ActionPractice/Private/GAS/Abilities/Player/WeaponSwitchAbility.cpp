#include "GAS/Abilities/Player/WeaponSwitchAbility.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/WeaponManagerComponent.h"
#include "Items/Weapon.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWeaponSwitchAbility, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogWeaponSwitchAbility, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UWeaponSwitchAbility::UWeaponSwitchAbility()
{
	bRetriggerInstancedAbility = true;
	StaminaCost = -1.0f;
	RotateTime = 0.0f;

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UWeaponSwitchAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	InputCycleRightWeaponTag = UGameplayTagsSubsystem::GetInputCycleRightWeaponTag();
	InputCycleLeftWeaponTag = UGameplayTagsSubsystem::GetInputCycleLeftWeaponTag();
}

void UWeaponSwitchAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//TriggerEventData에서 좌/우 판별
	bIsLeftHand = false;
	if (TriggerEventData)
	{
		if (TriggerEventData->InstigatorTags.HasTag(InputCycleLeftWeaponTag))
		{
			bIsLeftHand = true;
		}
	}

	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UWeaponManagerComponent* WeaponManager = Character->GetWeaponManagerComponent();
	if (!WeaponManager)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//1. 실제 무기 교체 (서버에서만 실행, HasAuthority 내부 체크)
	if (bIsLeftHand)
	{
		WeaponManager->CycleLeftWeapon();
		DEBUG_LOG(TEXT("WeaponSwitch: Cycle Left"));
	}
	else
	{
		WeaponManager->CycleRightWeapon();
		DEBUG_LOG(TEXT("WeaponSwitch: Cycle Right"));
	}

	//2. 새 무기 숨김 (몽타주 끝날 때 표시)
	SetSwitchingWeaponVisibility(false);

	//3. 오른손 무기 변경 시 AttackSequenceAbility 재활성화
	if (!bIsLeftHand)
	{
		Character->ResetAttackSequenceAbility();
	}

	//4. 교체 몽타주 재생
	StartMontageWithEventsTask();
}

void UWeaponSwitchAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	SetSwitchingWeaponVisibility(true);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UWeaponSwitchAbility::ConsumeStamina()
{
	return true;
}

UAnimMontage* UWeaponSwitchAbility::SetMontageToPlayTask()
{
	if (!WeaponSwitchMontage)
	{
		DEBUG_LOG(TEXT("SetMontageToPlayTask: No WeaponSwitchMontage"));
	}
	return WeaponSwitchMontage;
}

void UWeaponSwitchAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("WeaponSwitch Montage Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UWeaponSwitchAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("WeaponSwitch Montage Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UWeaponSwitchAbility::SetSwitchingWeaponVisibility(bool bVisible)
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	UWeaponManagerComponent* WeaponManager = Character->GetWeaponManagerComponent();
	if (!WeaponManager) return;

	AWeapon* Weapon = bIsLeftHand ? WeaponManager->GetLeftWeapon() : WeaponManager->GetRightWeapon();
	if (Weapon)
	{
		Weapon->SetActorHiddenInGame(!bVisible);
	}
}
