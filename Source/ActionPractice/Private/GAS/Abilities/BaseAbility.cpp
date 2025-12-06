#include "GAS/Abilities/BaseAbility.h"
#include "Characters/BaseCharacter.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBaseAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UBaseAbility::UBaseAbility()
{
	// 기본 설정
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

//Ability는 OnGiveAbility가 BeginPlay처럼 사용됨
void UBaseAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EffectStaminaCostTag = UGameplayTagsSubsystem::GetEffectStaminaCostTag();
	if (!EffectStaminaCostTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectStaminaCostTag is Invalid"));
	}

	EffectCooldownDurationTag = UGameplayTagsSubsystem::GetEffectCooldownDurationTag();
	if (!EffectCooldownDurationTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectCooldownDurationTag is Invalid"));
	}
}

bool UBaseAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	const bool bSuperCanActivate = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

	if (!bSuperCanActivate)
	{
		DEBUG_LOG(TEXT("BaseAbility::CanActivateAbility FAILED at Super. Ability=%s"), *GetName());
		return false;
	}

	const bool bHasStamina = CheckStaminaCost(*ActorInfo);
	if (!bHasStamina)
	{
		DEBUG_LOG(TEXT("BaseAbility::CanActivateAbility FAILED by Stamina. Ability=%s"), *GetName());
		return false;
	}

	return true;
}

void UBaseAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		DEBUG_LOG(TEXT("Cannot Commit Ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActivateInitSettings();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UBaseAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UBaseAbility::CheckStaminaCost(const FGameplayAbilityActorInfo& ActorInfo) const
{
	//0이면 스테미나 소모는 없지만 현재 스테미나가 있는지는 확인, 음수면 체크도 하지 않음
	if (StaminaCost < 0.0f)
	{
		return true;
	}

	if (const UBaseAttributeSet* AttributeSet = GetBaseAttributeSetFromActorInfo(&ActorInfo))
	{
		return AttributeSet->GetStamina() > 0;
	}

	return false;
}

bool UBaseAbility::ApplyStaminaCost()
{
	//0이면 스테미나 소모는 없지만 현재 스테미나가 있는지는 확인, 음수면 체크도 하지 않음
	if (StaminaCost < 0.0f)
	{
		return true;
	}

	const UBaseAttributeSet* AttributeSet = GetBaseAttributeSetFromActorInfo();
	if (!AttributeSet || AttributeSet->GetStamina() < 3.0f)
	{
		DEBUG_LOG(TEXT("No AttributeSet or Low Stamina"));
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StaminaCostEffect)
	{
		DEBUG_LOG(TEXT("No ASC or StaminaCostEffect"));
		return false;
	}

	//EffectSpec 생성
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle EffectSpec = ASC->MakeOutgoingSpec(StaminaCostEffect, EffectiveLevel, EffectContext);

	if (!EffectSpec.IsValid())
	{
		DEBUG_LOG(TEXT("Failed StaminaCost GameplayEffectSpec"));
		return false;
	}

	EffectSpec.Data.Get()->SetSetByCallerMagnitude(EffectStaminaCostTag, -StaminaCost);
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	const bool bApplied = Handle.IsValid();

	DEBUG_LOG(TEXT("ApplyStaminaCost applied=%s, Cost=%.2f"), bApplied ? TEXT("true") : TEXT("false"), StaminaCost);

	return true;
}

void UBaseAbility::SetStaminaCost(float InStaminaCost)
{
	StaminaCost = InStaminaCost;
}

UBaseAbilitySystemComponent* UBaseAbility::GetBaseAbilitySystemComponentFromActorInfo() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		return Cast<UBaseAbilitySystemComponent>(ASC);
	}
	return nullptr;
}

ABaseCharacter* UBaseAbility::GetBaseCharacterFromActorInfo() const
{
	return Cast<ABaseCharacter>(GetActorInfo().AvatarActor.Get());
}

ABaseCharacter* UBaseAbility::GetBaseCharacterFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get());
}

const UBaseAttributeSet* UBaseAbility::GetBaseAttributeSetFromActorInfo() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		return ASC->GetSet<UBaseAttributeSet>();
	}
	return nullptr;
}

const UBaseAttributeSet* UBaseAbility::GetBaseAttributeSetFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		return ASC->GetSet<UBaseAttributeSet>();
	}
	return nullptr;
}

const FGameplayTagContainer* UBaseAbility::GetCooldownTags() const
{
	//AbilityTags를 쿨다운 태그로 사용
	if (AbilityTags.IsValid() && AbilityTags.Num() > 0)
	{
		return &AbilityTags;
	}

	return Super::GetCooldownTags();
}

void UBaseAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	//CooldownDuration이 0 이하면 쿨다운을 건너뛰거나 Super만 호출
	if (CooldownDuration <= 0.0f)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	//쿨다운 GameplayEffect 클래스 얻기 (엔진 기본 속성 사용)
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE)
	{
		DEBUG_LOG(TEXT("No Cooldown GameplayEffect set"));
		return;
	}

	//ASC 확인
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		DEBUG_LOG(TEXT("No AbilitySystemComponent"));
		return;
	}

	//EffectContext 생성
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	//Spec 생성
	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CooldownGE->GetClass(), EffectiveLevel, EffectContext);

	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("Failed to create Cooldown GameplayEffectSpec"));
		return;
	}

	//SetByCaller로 Duration 주입
	if (EffectCooldownDurationTag.IsValid() && CooldownDuration > 0.0f)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(EffectCooldownDurationTag, CooldownDuration);
		DEBUG_LOG(TEXT("ApplyCooldown: Duration=%.2f"), CooldownDuration);
	}

	//AbilityTags를 동적으로 부여 (GetCooldownTags와 일치시킴)
	if (AbilityTags.IsValid() && AbilityTags.Num() > 0)
	{
		SpecHandle.Data->DynamicGrantedTags.AppendTags(AbilityTags);
		DEBUG_LOG(TEXT("ApplyCooldown: Added AbilityTags as Granted Tags"));
	}

	//자기 자신에게 적용
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}