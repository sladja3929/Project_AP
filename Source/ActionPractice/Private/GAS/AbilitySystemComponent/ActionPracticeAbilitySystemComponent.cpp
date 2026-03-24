#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/AttackData.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeAbilitySystemComponent, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeAbilitySystemComponent, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UActionPracticeAbilitySystemComponent::UActionPracticeAbilitySystemComponent()
{
}

void UActionPracticeAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		OwnerCharacter = Cast<AActionPracticeCharacter>(Owner);
	}

	EffectStaminaRegenBlockDurationTag = UGameplayTagsSubsystem::GetEffectStaminaRegenBlockDurationTag();
	if (!EffectStaminaRegenBlockDurationTag.IsValid())
	{
		DEBUG_LOG(TEXT("EffectStaminaRegenBlockDurationTag is Invalid"));
	}

	StateAbilityBlockingTag = UGameplayTagsSubsystem::GetStateAbilityBlockingTag();
	if (!StateAbilityBlockingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateAbilityBlockingTag is Invalid"));
	}

	StateGuardBrokenTag = UGameplayTagsSubsystem::GetStateGuardBrokenTag();
	if (!StateGuardBrokenTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateGuardBrokenTag is Invalid"));
	}
}

void UActionPracticeAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	OwnerCharacter = Cast<AActionPracticeCharacter>(InOwnerActor);
}

void UActionPracticeAbilitySystemComponent::HandleDeath()
{
	if (bDeathHandled) return;
	Super::HandleDeath();

	//서버 권한에서만 Death Ability 활성화
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

	//AbilityDeath 태그로 스펙 조회
	const FGameplayTag AbilityDeathTag = UGameplayTagsSubsystem::GetAbilityDeathTag();
	if (!AbilityDeathTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("APASC::HandleDeath - AbilityDeathTag is not valid"));
		return;
	}

	TArray<FGameplayAbilitySpec*> DeathSpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityDeathTag), DeathSpecs);
	if (DeathSpecs.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("APASC::HandleDeath - No Death ability spec found"));
		return;
	}

	//이미 활성화 중이면 중복 방지
	if (DeathSpecs[0]->IsActive())
	{
		DEBUG_LOG(TEXT("HandleDeath: PlayerDeathAbility already active"));
		return;
	}

	if (!TryActivateAbilityWithEventData(DeathSpecs[0]->Handle, nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("APASC::HandleDeath - Failed to activate PlayerDeathAbility"));
	}
}

void UActionPracticeAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	//LocalPredicted만 대상으로
	const UGameplayAbility* AbilityCDO = Spec.Ability;
	const bool bIsLocalPredicted = (AbilityCDO && AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);

	//Standalone/ListenServer 로컬/Client(Autonomous)에서만 태움
	const bool bIsLocallyControlled = AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled();
	if (!bIsLocalPredicted || !bIsLocallyControlled || !Spec.IsActive())
	{
		return;
	}

	TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
	if (Instances.IsEmpty() || !Instances.Last())
	{
		return;
	}

	const FGameplayAbilityActivationInfo& ActivationInfo = Instances.Last()->GetCurrentActivationInfoRef();
	const FPredictionKey OriginalPredictionKey = ActivationInfo.GetActivationPredictionKey();
	
	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, OriginalPredictionKey);
}

void UActionPracticeAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	//LocalPredicted만 대상으로
	const UGameplayAbility* AbilityCDO = Spec.Ability;
	const bool bIsLocalPredicted = (AbilityCDO && AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);

	//Standalone/ListenServer 로컬/Client(Autonomous)에서만 태움
	const bool bIsLocallyControlled = AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled();
	if (!bIsLocalPredicted || !bIsLocallyControlled || !Spec.IsActive())
	{
		return;
	}

	TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
	if (Instances.IsEmpty() || !Instances.Last())
	{
		return;
	}

	const FGameplayAbilityActivationInfo& ActivationInfo = Instances.Last()->GetCurrentActivationInfoRef();
	const FPredictionKey OriginalPredictionKey = ActivationInfo.GetActivationPredictionKey();

	InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, OriginalPredictionKey);
}

void UActionPracticeAbilitySystemComponent::HandleGameplayEvent_NetPredicted(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (!EventTag.IsValid() || !Payload || !OwnerCharacter) return;

	//로컬 -> 로컬, 서버 -> 서버 각자에서는 기본 이벤트 발신
	HandleGameplayEvent(EventTag, Payload);
	DEBUG_LOG(TEXT("APASC: HandleGameplayEvent called - %s"), *EventTag.ToString());
	
	//로컬에서 서버로 RPC
	if (!OwnerCharacter->HasAuthority())
	{
		FGameplayEventData_NetPredicted NetPayload;
		NetPayload.EventTag = EventTag;
		NetPayload.InstigatorTags = Payload->InstigatorTags;
		NetPayload.EventMagnitude = Payload->EventMagnitude;
	
		Server_HandleGameplayEvent(NetPayload);
		DEBUG_LOG(TEXT("APASC: Server HandleGameplayEvent called - %s"), *EventTag.ToString());
	}
}

void UActionPracticeAbilitySystemComponent::Server_HandleGameplayEvent_Implementation(const FGameplayEventData_NetPredicted& Payload)
{
	if (!Payload.EventTag.IsValid()) return;

	FGameplayEventData EventData;
	EventData.EventTag = Payload.EventTag;
	EventData.InstigatorTags = Payload.InstigatorTags;
	EventData.EventMagnitude = Payload.EventMagnitude;

	HandleGameplayEvent(EventData.EventTag, &EventData);
}

void UActionPracticeAbilitySystemComponent::AddTag_NetPredicted(FGameplayTag AuthTag, FGameplayTag LocalTag)
{
	if (!OwnerCharacter) return;

	//서버: 권위 태그 부여
	if (OwnerCharacter->HasAuthority() && AuthTag.IsValid())
	{
		DEBUG_LOG(TEXT("Adding %s on Authority"), *AuthTag.ToString());
		AddLooseGameplayTag(AuthTag);
		AddMinimalReplicationGameplayTag(AuthTag);
	}

	//클라: 로컬 태그 부여 (로컬 입력 제어용)
	if (OwnerCharacter->IsLocallyControlled() && LocalTag.IsValid())
	{
		DEBUG_LOG(TEXT("Adding %s on Local"), *LocalTag.ToString());
		AddLooseGameplayTag(LocalTag);
	}
}

void UActionPracticeAbilitySystemComponent::RemoveTags_NetPredicted(FGameplayTag AuthTag, FGameplayTag LocalTag)
{
	if (!OwnerCharacter) return;

	//서버: 권위 태그 삭제
	if (OwnerCharacter->HasAuthority() && AuthTag.IsValid())
	{
		while (HasMatchingGameplayTag(AuthTag))
		{
			RemoveLooseGameplayTag(AuthTag);
			RemoveMinimalReplicationGameplayTag(AuthTag);
		}
		DEBUG_LOG(TEXT("Remove All %s on Authority"), *AuthTag.ToString());
	}

	//클라: 로컬 태그 삭제 (로컬 입력 제어용)
	if (OwnerCharacter->IsLocallyControlled() && LocalTag.IsValid())
	{
		while (HasMatchingGameplayTag(LocalTag))
		{
			RemoveLooseGameplayTag(LocalTag);
		}
		DEBUG_LOG(TEXT("Remove All %s on Local"), *LocalTag.ToString());
	}
}

const UActionPracticeAttributeSet* UActionPracticeAbilitySystemComponent::GetActionPracticeAttributeSet() const 
{
	return this->GetSet<UActionPracticeAttributeSet>();
}

void UActionPracticeAbilitySystemComponent::CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	CheckBlockSuccess(SourceActor);
	bGuardBroken = false;

	if (!OwnerCharacter)
	{
		Super::CalculateAndSetAttributes(SourceActor, FinalAttackData);
		return;
	}

	UActionPracticeAttributeSet* APAttributeSet = const_cast<UActionPracticeAttributeSet*>(GetActionPracticeAttributeSet());
	if (!APAttributeSet)
	{
		Super::CalculateAndSetAttributes(SourceActor, FinalAttackData);
		return;
	}

	//방어 성공 시 계산
	if (bBlockedLastAttack)
	{
		AWeapon* LeftWeapon = OwnerCharacter->GetLeftWeapon();
		const FBlockActionData* BlockData = LeftWeapon ? LeftWeapon->GetWeaponBlockData() : nullptr;
		const float DamageReduction = BlockData ? BlockData->DamageReduction : 0.0f;
		const float FinalDamage = FinalAttackData.FinalDamage * (1.0f - DamageReduction / 100.0f);

		//HP 적용
		const float OldHealth = APAttributeSet->GetHealth();
		APAttributeSet->SetHealth(FMath::Clamp(OldHealth - FinalDamage, 0.0f, APAttributeSet->GetMaxHealth()));

		//가드 성공 시 포이즈 대미지는 적용하지 않음

		//===== 가드 스태미나 소모 (엘든링 Guard Boost 공식) =====
		if (FinalAttackData.PoiseDamage > 0.0f && BlockData)
		{
			const float GuardStrength = FMath::Clamp(BlockData->GuardStrength, 0.0f, 100.0f);
			const float StaminaCost = FinalAttackData.PoiseDamage * (1.0f - GuardStrength / 100.0f);

			if (StaminaCost > 0.0f)
			{
				const float OldStamina = APAttributeSet->GetStamina();
				const float NewStamina = FMath::Max(0.0f, OldStamina - StaminaCost);
				APAttributeSet->SetStamina(NewStamina);

				DEBUG_LOG(TEXT("Guard Stamina: PoiseDmg=%.1f, GuardStr=%.1f, Cost=%.1f, Stamina=%.1f->%.1f"),
					FinalAttackData.PoiseDamage, GuardStrength, StaminaCost, OldStamina, NewStamina);

				if (NewStamina <= 0.0f)
				{
					bGuardBroken = true;
					DEBUG_LOG(TEXT("Guard Break! Stamina depleted"));
				}
			}
		}

		DEBUG_LOG(TEXT("Blocked: Damage=%.1f, FinalDamage=%.1f, Reduction=%.1f%%, Health=%.1f/%.1f, GuardBroken=%d"),
			FinalAttackData.FinalDamage, FinalDamage, DamageReduction,
			APAttributeSet->GetHealth(), APAttributeSet->GetMaxHealth(), bGuardBroken);
		return;
	}

	//기본 or 방어실패: 기본 피격 계산식 사용
	Super::CalculateAndSetAttributes(SourceActor, FinalAttackData);
}

void UActionPracticeAbilitySystemComponent::PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData)
{
	Super::PrepareHitReactionEventData(OutEventData, FinalAttackData);

	if (bGuardBroken)
	{
		//가드 브레이크: State.GuardBroken 태그 전달
		OutEventData.TargetTags.AddTag(StateGuardBrokenTag);
		DEBUG_LOG(TEXT("GuardBreak Reaction: State.GuardBroken tag added"));
	}
	else if (bBlockedLastAttack)
	{
		//일반 블록 리액션: State.Blocking 태그 전달
		OutEventData.TargetTags.AddTag(StateAbilityBlockingTag);
		DEBUG_LOG(TEXT("Block Reaction triggered"));
	}
}

EDefenseResult UActionPracticeAbilitySystemComponent::GetDefenseResult() const
{
	if (bGuardBroken)
	{
		return EDefenseResult::GuardBroken;
	}
	if (bBlockedLastAttack)
	{
		return EDefenseResult::Blocked;
	}
	return EDefenseResult::None;
}

bool UActionPracticeAbilitySystemComponent::ShouldActivateHitReaction() const
{
	//가드 + 가드 브레이크 시 즉시 활성화
	if (bBlockedLastAttack || bGuardBroken)
	{
		return true;
	}

	//기본 포이즈 브레이크 체크
	return Super::ShouldActivateHitReaction();
}

void UActionPracticeAbilitySystemComponent::CheckBlockSuccess(AActor* SourceActor)
{
	bBlockedLastAttack = false;

	if (!OwnerCharacter || !SourceActor)
	{
		return;
	}

	//방어 태그 확인
	const bool bIsBlocking = HasMatchingGameplayTag(StateAbilityBlockingTag);

	if (!bIsBlocking || !OwnerCharacter->GetLeftWeapon())
	{
		return;
	}

	//공격자 방향 계산
	const FVector ToSource = SourceActor->GetActorLocation() - OwnerCharacter->GetActorLocation();
	const FVector ToSourceNormalized = ToSource.GetSafeNormal2D();
	const FVector Forward = OwnerCharacter->GetActorForwardVector();

	//캐릭터 정면과 공격 방향 사이의 각도 계산
	const float DotProduct = FVector::DotProduct(Forward, ToSourceNormalized);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	//방어 각도 범위
	const float BlockingAngle = 90.0f;

	if (AngleDegrees <= BlockingAngle)
	{
		bBlockedLastAttack = true;
	}
}

void UActionPracticeAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}