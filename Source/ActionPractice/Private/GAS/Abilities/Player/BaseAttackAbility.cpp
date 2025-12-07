#include "GAS/Abilities/Player/BaseAttackAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Items/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/Abilities/Player/WeaponAbilityStatics.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBaseAttackAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseAttackAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UBaseAttackAbility::UBaseAttackAbility()
{
    StaminaCost = 15.0f;
}

void UBaseAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
    if (!Character)
    {
        DEBUG_LOG(TEXT("No Character"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    
    //HitDetectionSetter 초기화
    if (!HitDetectionSetter.Init(Character->GetHitDetectionInterface()))
    {
        DEBUG_LOG(TEXT("Failed to init HitDetectionSetter"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    //HitDetectionSetter 바인딩
    if (!HitDetectionSetter.Bind(this))
    {
        DEBUG_LOG(TEXT("Failed to bind HitDetectionSetter"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UBaseAttackAbility::SetHitDetectionConfig()
{
    FGameplayTagContainer AssetTag = GetAssetTags();
    if (AssetTag.IsEmpty())
    {
        DEBUG_LOG(TEXT("No AssetTags"));
        return;
    }
    
    //PrepareHitDetection 호출
    if (!HitDetectionSetter.PrepareHitDetection(AssetTag, ComboCounter))
    {
        DEBUG_LOG(TEXT("Failed to prepare HitDetection"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    DEBUG_LOG(TEXT("Attack Ability: Call Hit Detection Prepare"));
}

void UBaseAttackAbility::OnHitDetected(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData AttackData)
{
    //Source ASC (공격자, AttackAbility 소유자)
    UActionPracticeAbilitySystemComponent* SourceASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
    if (!HitActor || !SourceASC) return;
    
    //Target ASC (피격자)
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
    if (!TargetASC) return;
    
    //Source ASC에서 GE Spec 생성
    FGameplayEffectSpecHandle SpecHandle = SourceASC->CreateAttackGameplayEffectSpec(DamageInstantEffect, GetAbilityLevel(), this, AttackData);
    
    if (SpecHandle.IsValid())
    {        
        //Target에게 적용
        FActiveGameplayEffectHandle ActiveGEHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        
        //적용에 성공했으면
        if (ActiveGEHandle.WasSuccessfullyApplied())
        {
            //추후 필요시 구현
        }
    }
}

void UBaseAttackAbility::ActivateInitSettings()
{
    Super::ActivateInitSettings();
    
    WeaponAttackData = FWeaponAbilityStatics::GetAttackDataFromAbility(this);
    if (!WeaponAttackData)
    {
        DEBUG_LOG(TEXT("Cannot Load Base Attack Data"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    ComboCounter = 0;
}

bool UBaseAttackAbility::ConsumeStamina()
{
    SetStaminaCost(WeaponAttackData->ComboSequence[ComboCounter].AttackData.StaminaCost);

    return Super::ConsumeStamina();
}

void UBaseAttackAbility::PlayAction()
{
    SetHitDetectionConfig();

    Super::PlayAction();
}

UAnimMontage* UBaseAttackAbility::SetMontageToPlayTask()
{
    if (!WeaponAttackData)
    {
        DEBUG_LOG(TEXT("SetMontageToPlayTask: No WeaponAttackData"));
        return nullptr;
    }

    if (ComboCounter < 0)
    {
        ComboCounter = 0;
    }

    // 소프트 레퍼런스를 실제 오브젝트로 로드
    const auto& ComboData = WeaponAttackData->ComboSequence[ComboCounter];
    UAnimMontage* Montage = ComboData.AttackMontage.LoadSynchronous();
    if (!Montage)
    {
        DEBUG_LOG(TEXT("SetMontageToPlayTask: Failed to load montage. ComboIndex=%d"), ComboCounter);
        return nullptr;
    }

    return Montage;
}

void UBaseAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    DEBUG_LOG(TEXT("EndAbility %d"), bWasCancelled);

    if (IsEndAbilityValid(Handle, ActorInfo))
    {
        //HitDetectionSetter 언바인딩
        HitDetectionSetter.UnBind();

        if (PlayMontageWithEventsTask)
        {
            PlayMontageWithEventsTask->bStopMontageWhenAbilityCancelled = bWasCancelled;
        }

        Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    }
}
