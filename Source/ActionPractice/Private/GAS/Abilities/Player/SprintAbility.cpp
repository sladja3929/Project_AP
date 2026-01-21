#include "GAS/Abilities/Player/SprintAbility.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogSprintAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogSprintAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

USprintAbility::USprintAbility()
{
	StaminaCost = 0.1f;
	SprintSpeedMultiplier = 1.5f;
}

void USprintAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	EffectSprintSpeedMultiplierTag = UGameplayTagsSubsystem::GetEffectSprintSpeedMultiplierTag();
	if (!EffectSprintSpeedMultiplierTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectSprintSpeedMultiplierTag is Invalid"));
	}
}

void USprintAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	DEBUG_LOG(TEXT("Sprint Ability Activated"));
	StartSprinting();
	StartWaitInputReleaseTask(true);
}

void USprintAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();
	
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character)
	{
		return;
	}

	SprintSpeedMultiplier = Character->SprintSpeedMultiplier;
}

void USprintAbility::StartSprinting()
{
	if (!StartStaminaDrainEffect())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!StartSprintEffect())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	//스프린트 조건 확인 타이머
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			SprintCheckTimer,
			this,
			&USprintAbility::CheckSprintConditions,
			0.1f,
			true
		);
	}

	DEBUG_LOG(TEXT("Sprint started"));
}

void USprintAbility::StopSprinting()
{
	StopSprintEffect();
	StopStaminaDrainEffect();
	
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SprintCheckTimer);
	}

	DEBUG_LOG(TEXT("Sprint ended"));
}

void USprintAbility::HandleSprinting()
{
	// 스프린트 중 추가 처리가 필요할때
}

bool USprintAbility::CanContinueSprinting() const
{
	UActionPracticeAttributeSet* AttributeSet = GetActionPracticeAttributeSetFromActorInfo();
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();

	if (!AttributeSet || !Character)
	{
		return false;
	}

	//스테미나 부족
	if (AttributeSet->GetStamina() <= 0)
	{
		DEBUG_LOG(TEXT("CanContinueSprinting Stop - No Stamina"));
		return false;
	}

	//이동 입력이 없으면
	FVector2D MovementInput = Character->GetCurrentMovementInput();
	DEBUG_LOG(TEXT("Real-time MovementInput: %f"), MovementInput.Size());
	if (MovementInput.Size() < 0.1f)
	{
		DEBUG_LOG(TEXT("CanContinueSprinting Stop - No Movement Input"));
		return false;
	}

	//공중에 있으면
	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (MovementComp && MovementComp->IsFalling())
	{
		DEBUG_LOG(TEXT("CanContinueSprinting Stop - Is Falling"));
		return false;
	}

	return true;
}

bool USprintAbility::StartSprintEffect()
{
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("No APASC"));
		return false;
	}

	// 기존 이펙트가 살아있으면 재설정(해제 후 재적용)
	if (SprintHandle.IsValid())
	{
		APASC->RemoveActiveGameplayEffect(SprintHandle);
		SprintHandle = FActiveGameplayEffectHandle();
	}

	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle SpecHandle = APASC->CreateGameplayEffectSpec(SprintEffect, EffectiveLevel, this);

	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("failed Sprint GameplayEffectSpec"));
		return false;
	}

	APASC->SetSpecSetByCallerMagnitude(SpecHandle, EffectSprintSpeedMultiplierTag, SprintSpeedMultiplier);
	SprintHandle = APASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	const bool bApplied = SprintHandle.IsValid();

	DEBUG_LOG(TEXT("SprintEffect applied=%s, SpeedMultiplier=%.2f"), bApplied ? TEXT("true") : TEXT("false"), SprintSpeedMultiplier);

	return bApplied;
}

void USprintAbility::StopSprintEffect()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		DEBUG_LOG(TEXT("No ASC"));
		SprintHandle = FActiveGameplayEffectHandle();
		return;
	}

	if (SprintHandle.IsValid())
	{
		const int32 Removed = ASC->RemoveActiveGameplayEffect(SprintHandle);
		SprintHandle = FActiveGameplayEffectHandle();
		DEBUG_LOG(TEXT("SprintEffect removed=%d"), Removed);
	}
}

bool USprintAbility::StartStaminaDrainEffect()
{
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("No APASC"));
		return false;
	}

	// 기존 드레인이 살아있으면 재설정(해제 후 재적용)
	if (StaminaDrainHandle.IsValid())
	{
		APASC->RemoveActiveGameplayEffect(StaminaDrainHandle);
		StaminaDrainHandle = FActiveGameplayEffectHandle();
	}

	const float EffectiveLevel = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle SpecHandle = APASC->CreateGameplayEffectSpec(StaminaDrainEffect, EffectiveLevel, this);

	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("failed StaminaDrain GameplayEffectSpec"));
		return false;
	}

	APASC->SetSpecSetByCallerMagnitude(SpecHandle, EffectStaminaCostTag, -StaminaCost);
	StaminaDrainHandle = APASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	const bool bApplied = StaminaDrainHandle.IsValid();

	DEBUG_LOG(TEXT("StaminaDrainEffect applied=%s, DrainPerPeriod=%.2f"), bApplied ? TEXT("true") : TEXT("false"), StaminaCost);

	return bApplied;
}

void USprintAbility::StopStaminaDrainEffect()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		DEBUG_LOG(TEXT("No ASC"));
		StaminaDrainHandle = FActiveGameplayEffectHandle();
		return;
	}

	if (StaminaDrainHandle.IsValid())
	{
		const int32 Removed = ASC->RemoveActiveGameplayEffect(StaminaDrainHandle);
		StaminaDrainHandle = FActiveGameplayEffectHandle();
		DEBUG_LOG(TEXT("StaminaDrainEffect removed=%d"), Removed);
	}
}

void USprintAbility::CheckSprintConditions()
{
	if (!CanContinueSprinting())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void USprintAbility::HandleWaitInputReleased(float TimeHeld)
{
	DEBUG_LOG(TEXT("Sprint WaitInputRelease - End Ability (TimeHeld: %.3f)"), TimeHeld);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USprintAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	DEBUG_LOG(TEXT("sprint cancel"));
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void USprintAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopSprinting();
	DEBUG_LOG(TEXT("sprint end"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
