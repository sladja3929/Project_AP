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

bool USprintAbility::ShouldRunSprintChecks() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();

	const bool bIsAuthority = (Avatar && Avatar->HasAuthority());
	const bool bIsLocallyControlled = (Character && Character->IsLocallyControlled());

	return bIsAuthority || bIsLocallyControlled;
}

void USprintAbility::StartSprinting()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const bool bIsAuthority = (Avatar && Avatar->HasAuthority());

	//속도 증가 이펙트: 클라/서버 모두 시도 (예측용)
	const bool bSprintApplied = StartSprintEffect();
	if (bIsAuthority && !bSprintApplied)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	else if (!bSprintApplied)
	{
		DEBUG_LOG(TEXT("SprintEffect apply failed on non-authority (will not end ability)."));
	}
	
	//스태미나 소모 이펙트: 서버에서만 필수
	if (bIsAuthority)
	{
		if (!StartStaminaDrainEffect())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
	}
	
	//체크 타이머: Authority 또는 LocallyControlled에서만 실행
	if (ShouldRunSprintChecks() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(SprintCheckTimer, this, &USprintAbility::CheckSprintConditions, 0.1f, true);
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
	if (AttributeSet->GetStamina() <= 0.0f)
	{
		DEBUG_LOG(TEXT("CanContinueSprinting Stop - No Stamina"));
		return false;
	}

	//공중에 있으면
	UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
	if (MovementComp && MovementComp->IsFalling())
	{
		DEBUG_LOG(TEXT("CanContinueSprinting Stop - Is Falling"));
		return false;
	}

	//이동 입력이 없으면: 클라/서버 분기
	//서버: 물리 상태 기반
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar && Avatar->HasAuthority())
	{
		if (!MovementComp)
		{
			return false;
		}

		const float Accel2D = MovementComp->GetCurrentAcceleration().Size2D();
		const float Speed2D = MovementComp->Velocity.Size2D();

		DEBUG_LOG(TEXT("Server/Proxy MoveCheck Accel2D=%.2f Speed2D=%.2f"), Accel2D, Speed2D);

		if (Accel2D <= ServerMinAccelerationToKeepSprint && Speed2D <= ServerMinSpeedToKeepSprint)
		{
			DEBUG_LOG(TEXT("CanContinueSprinting Stop - Not Moving (Authority)"));
			return false;
		}

		return true;
	}
	
	//로컬: 입력 기반
	if (Character->IsLocallyControlled())
	{
		const FVector2D MovementInput = Character->GetCurrentMovementInput();
		DEBUG_LOG(TEXT("Real-time MovementInput %.3f"), MovementInput.Size());

		if (MovementInput.Size() <= MovementInputThreshold)
		{
			DEBUG_LOG(TEXT("CanContinueSprinting Stop - No Movement Input (Local)"));
			return false;
		}

		return true;
	}

	//나머지(시뮬 프록시 등)는 기존 Server/Proxy 로직을 유지하거나,
	//보수적으로 true/false 중 정책 결정 가능. (여기서는 기존 물리판정 유지)
	if (!MovementComp)
	{
		return false;
	}

	const float Accel2D = MovementComp->GetCurrentAcceleration().Size2D();
	const float Speed2D = MovementComp->Velocity.Size2D();

	DEBUG_LOG(TEXT("Server/Proxy MoveCheck Accel2D=%.2f Speed2D=%.2f"), Accel2D, Speed2D);

	if (Accel2D <= ServerMinAccelerationToKeepSprint && Speed2D <= ServerMinSpeedToKeepSprint)
	{
		DEBUG_LOG(TEXT("CanContinueSprinting Stop - Not Moving (Server/Proxy)"));
		return false;
	}

	return true;
}

void USprintAbility::CheckSprintConditions()
{
	if (!ShouldRunSprintChecks())
	{
		return;
	}

	if (!CanContinueSprinting())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

bool USprintAbility::StartSprintEffect()
{
	UActionPracticeAbilitySystemComponent* APASC = GetActionPracticeAbilitySystemComponentFromActorInfo();
	if (!APASC)
	{
		DEBUG_LOG(TEXT("No APASC"));
		return false;
	}

	//기존 이펙트가 살아있으면 재설정(해제 후 재적용)
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
	//드레인은 서버 권한으로만 운영
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority())
	{
		DEBUG_LOG(TEXT("StartStaminaDrainEffect skipped on non-authority."));
		return true;
	}
	
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

void USprintAbility::OnWaitInputRelease(float TimeHeld)
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
