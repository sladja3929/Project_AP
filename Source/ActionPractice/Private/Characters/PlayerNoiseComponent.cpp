#include "Characters/PlayerNoiseComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Perception/AISense_Hearing.h"
#include "TimerManager.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogPlayerNoiseComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogPlayerNoiseComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

namespace PlayerNoise
{
	//노이즈 종류 (AIStimulus Tag, 비주얼 로거·디버깅 구분용)
	static const FName MovementTag(TEXT("Movement"));
	static const FName CombatTag(TEXT("Combat"));
}

UPlayerNoiseComponent::UPlayerNoiseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerNoiseComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();

	//AI 인지는 서버에서만 동작
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	DamageTag = UGameplayTagsSubsystem::GetEffectDamageIncomingDamageTag();
	UndetectableTag = UGameplayTagsSubsystem::GetStateUndetectableTag();
	DeadTag = UGameplayTagsSubsystem::GetStateDeadTag();

	//ASC는 캐릭터 생성자에서 만들어지므로 BeginPlay 시점에 존재 (GE 적용 델리게이트는 서버에서만 브로드캐스트)
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		CachedASC = ASC;
		AppliedToTargetHandle = ASC->OnGameplayEffectAppliedDelegateToTarget.AddUObject(this, &UPlayerNoiseComponent::OnEffectAppliedToTarget);
		AppliedToSelfHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UPlayerNoiseComponent::OnEffectAppliedToSelf);
	}

	GetWorld()->GetTimerManager().SetTimer(MovementNoiseTimer, this, &UPlayerNoiseComponent::EmitMovementNoise, MovementNoiseInterval, true);
}

void UPlayerNoiseComponent::EmitMovementNoise()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* MovementComp = Character ? Character->GetCharacterMovement() : nullptr;

	if (!MovementComp || !MovementComp->IsMovingOnGround() || !CanMakeNoise())
	{
		return;
	}

	const float Speed = MovementComp->Velocity.Size2D();

	if (Speed < MinMovementNoiseSpeed)
	{
		return;
	}

	const float Loudness = FMath::GetMappedRangeValueClamped(
		FVector2f(MinMovementNoiseSpeed, FMath::Max(LoudestMovementSpeed, MinMovementNoiseSpeed + 1.0f)),
		FVector2f(MinMovementLoudness, MaxMovementLoudness),
		Speed);

	//발밑 위치
	const FVector FootLocation = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight());
	ReportNoise(FootLocation, Loudness, PlayerNoise::MovementTag);
}

void UPlayerNoiseComponent::OnEffectAppliedToTarget(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	//자기 자신에게 적용한 GE는 Self 델리게이트에서 처리 (중복 발생 방지)
	if (!TargetASC || TargetASC == CachedASC.Get() || !IsDamageSpec(Spec) || !CanMakeNoise())
	{
		return;
	}

	//적중 지점(피격자 위치)에서 발생. 자극 대상은 플레이어라 들은 적은 플레이어를 인지
	const AActor* HitActor = TargetASC->GetAvatarActor();
	ReportNoise(HitActor ? HitActor->GetActorLocation() : GetOwner()->GetActorLocation(), CombatLoudness, PlayerNoise::CombatTag);
}

void UPlayerNoiseComponent::OnEffectAppliedToSelf(UAbilitySystemComponent* SourceASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	if (!IsDamageSpec(Spec) || !CanMakeNoise())
	{
		return;
	}

	ReportNoise(GetOwner()->GetActorLocation(), CombatLoudness, PlayerNoise::CombatTag);
}

bool UPlayerNoiseComponent::IsDamageSpec(const FGameplayEffectSpec& Spec) const
{
	return DamageTag.IsValid() && Spec.SetByCallerTagMagnitudes.Contains(DamageTag);
}

bool UPlayerNoiseComponent::CanMakeNoise() const
{
	const UAbilitySystemComponent* ASC = CachedASC.Get();

	if (!ASC)
	{
		return true;
	}

	const bool bUndetectable = UndetectableTag.IsValid() && ASC->HasMatchingGameplayTag(UndetectableTag);
	const bool bDead = DeadTag.IsValid() && ASC->HasMatchingGameplayTag(DeadTag);

	return !bUndetectable && !bDead;
}

void UPlayerNoiseComponent::ReportNoise(const FVector& Location, float Loudness, FName Tag) const
{
	if (Loudness <= 0.0f)
	{
		return;
	}

	DEBUG_LOG(TEXT("ReportNoise: %s Loudness=%.2f"), *Tag.ToString(), Loudness);	UAISense_Hearing::ReportNoiseEvent(GetOwner(), Location, Loudness, GetOwner(), 0.0f, Tag);
}

void UPlayerNoiseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementNoiseTimer);
	}

	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->OnGameplayEffectAppliedDelegateToTarget.Remove(AppliedToTargetHandle);
		ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(AppliedToSelfHandle);
	}

	CachedASC.Reset();

	Super::EndPlay(EndPlayReason);
}
