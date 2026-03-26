#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Characters/ActionPracticeCharacter.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/AttackData.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

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

	StateParryingTag = UGameplayTagsSubsystem::GetStateParryingTag();
	if (!StateParryingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateParryingTag is Invalid"));
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
	LastDefenseState = EPlayerDefenseState::None;

	//패리 → 가드 → 일반 피격 우선순위
	CheckParrySuccess(SourceActor, FinalAttackData);
	if (LastDefenseState == EPlayerDefenseState::Parried)
	{
		DEBUG_LOG(TEXT("CalculateAndSetAttributes: Parried — all damage ignored, forcing enemy groggy"));
		ForceEnemyGroggy(SourceActor);
		return;
	}

	CheckBlockSuccess(SourceActor);
	if (LastDefenseState != EPlayerDefenseState::None)
	{
		//패리 어빌리티 활성 중인지 확인 → 폴백 여부 판정
		bool bIsParryActive = false;
		const FGameplayTag AbilityParryTag = UGameplayTagsSubsystem::GetAbilityParryTag();
		if (AbilityParryTag.IsValid())
		{
			for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
			{
				if (Spec.IsActive() && Spec.Ability && Spec.Ability->AbilityTags.HasTag(AbilityParryTag))
				{
					bIsParryActive = true;
					break;
				}
			}
		}

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

		AWeapon* LeftWeapon = OwnerCharacter->GetLeftWeapon();
		const FBlockActionData* BlockData = LeftWeapon ? LeftWeapon->GetWeaponBlockData() : nullptr;
		const float DamageReduction = BlockData ? BlockData->DamageReduction : 0.0f;
		const float FinalDamage = FinalAttackData.FinalDamage * (1.0f - DamageReduction / 100.0f);

		//HP 적용 
		const float OldHealth = APAttributeSet->GetHealth();
		APAttributeSet->SetHealth(FMath::Clamp(OldHealth - FinalDamage, 0.0f, APAttributeSet->GetMaxHealth()));

		//가드 성공 시 포이즈 대미지는 적용하지 않음

		//===== 가드 스태미나 소모 (엘든링 가드 공식) =====
		bool bGuardBroken = false;
		if (FinalAttackData.PoiseDamage > 0.0f && BlockData)
		{
			const float GuardStrength = FMath::Clamp(BlockData->GuardStrength, 0.0f, 100.0f);
			const float ParryFallbackMultiplier = bIsParryActive ? 4.0f : 1.0f;
			const float StaminaCost = FinalAttackData.PoiseDamage * (1.0f - GuardStrength / 100.0f) * ParryFallbackMultiplier;

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

		//최종 상태 결정
		if (bGuardBroken)
		{
			LastDefenseState = bIsParryActive ? EPlayerDefenseState::ParryFallbackGuardBroken : EPlayerDefenseState::GuardBroken;
		}
		else
		{
			LastDefenseState = bIsParryActive ? EPlayerDefenseState::ParryFallbackBlocked : EPlayerDefenseState::Blocked;
		}

		DEBUG_LOG(TEXT("CalculateAndSetAttributes: DefenseState=%d, Damage=%.1f, FinalDamage=%.1f, Reduction=%.1f%%, Health=%.1f/%.1f, ParryActive=%d"),
			static_cast<uint8>(LastDefenseState), FinalAttackData.FinalDamage, FinalDamage, DamageReduction,
			APAttributeSet->GetHealth(), APAttributeSet->GetMaxHealth(), bIsParryActive);
		return;
	}

	//기본 or 방어실패: 기본 피격 계산식 사용
	Super::CalculateAndSetAttributes(SourceActor, FinalAttackData);
}

void UActionPracticeAbilitySystemComponent::PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData)
{
	Super::PrepareHitReactionEventData(OutEventData, FinalAttackData);

	switch (LastDefenseState)
	{
	case EPlayerDefenseState::GuardBroken:
	case EPlayerDefenseState::ParryFallbackGuardBroken:
		OutEventData.TargetTags.AddTag(StateGuardBrokenTag);
		DEBUG_LOG(TEXT("GuardBreak Reaction: State.GuardBroken tag added"));
		break;

	case EPlayerDefenseState::Blocked:
		OutEventData.TargetTags.AddTag(StateAbilityBlockingTag);
		DEBUG_LOG(TEXT("Block Reaction triggered"));
		break;

	default:
		break;
	}
}

EDefenseResult UActionPracticeAbilitySystemComponent::GetDefenseResult() const
{
	switch (LastDefenseState)
	{
	case EPlayerDefenseState::Parried:
		return EDefenseResult::Parried;

	case EPlayerDefenseState::GuardBroken:
	case EPlayerDefenseState::ParryFallbackGuardBroken:
		return EDefenseResult::GuardBroken;

	case EPlayerDefenseState::Blocked:
	case EPlayerDefenseState::ParryFallbackBlocked:
		return EDefenseResult::Blocked;

	default:
		return EDefenseResult::None;
	}
}

bool UActionPracticeAbilitySystemComponent::ShouldActivateHitReaction() const
{
	const bool bSuperResult = Super::ShouldActivateHitReaction();
	DEBUG_LOG(TEXT("ShouldActivateHitReaction: DefenseState=%d, SuperResult=%d"),
		static_cast<uint8>(LastDefenseState), bSuperResult);

	switch (LastDefenseState)
	{
	case EPlayerDefenseState::Parried:
		DEBUG_LOG(TEXT("ShouldActivateHitReaction: FALSE (parried)"));
		return false;

	case EPlayerDefenseState::ParryFallbackBlocked:
		DEBUG_LOG(TEXT("ShouldActivateHitReaction: FALSE (parry fallback block)"));
		return false;

	case EPlayerDefenseState::Blocked:
	case EPlayerDefenseState::GuardBroken:
	case EPlayerDefenseState::ParryFallbackGuardBroken:
		DEBUG_LOG(TEXT("ShouldActivateHitReaction: TRUE (DefenseState=%d)"), static_cast<uint8>(LastDefenseState));
		return true;

	default:
		break;
	}

	//기본 포이즈 브레이크 체크
	DEBUG_LOG(TEXT("ShouldActivateHitReaction: SuperResult=%d"), bSuperResult);
	return bSuperResult;
}

void UActionPracticeAbilitySystemComponent::CheckParrySuccess(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	if (!OwnerCharacter || !SourceActor)
	{
		DEBUG_LOG(TEXT("CheckParrySuccess: SKIP (no owner or source)"));
		return;
	}

	//패리 불가 공격
	if (FinalAttackData.bUnparriable)
	{
		DEBUG_LOG(TEXT("CheckParrySuccess: SKIP (unparriable)"));
		return;
	}

	//State.Parrying 태그 확인
	const bool bHasParryingTag = HasMatchingGameplayTag(StateParryingTag);
	DEBUG_LOG(TEXT("CheckParrySuccess: StateParryingTag valid=%d, has=%d"), StateParryingTag.IsValid(), bHasParryingTag);
	if (!bHasParryingTag)
	{
		DEBUG_LOG(TEXT("CheckParrySuccess: SKIP (no State.Parrying tag)"));
		return;
	}

	//좌측 무기(방패/무기) 확인
	AWeapon* LeftWeapon = OwnerCharacter->GetLeftWeapon();
	if (!LeftWeapon)
	{
		DEBUG_LOG(TEXT("CheckParrySuccess: SKIP (no LeftWeapon)"));
		return;
	}

	//패리 각도 확인 (WeaponDataAsset의 BlockData에서 읽기)
	const FBlockActionData* BlockData = LeftWeapon->GetWeaponBlockData();
	const float ParryAngle = BlockData ? BlockData->ParryAngle : 60.0f;

	//공격자 방향 계산
	const FVector ToSource = SourceActor->GetActorLocation() - OwnerCharacter->GetActorLocation();
	const FVector ToSourceNormalized = ToSource.GetSafeNormal2D();
	const FVector Forward = OwnerCharacter->GetActorForwardVector();

	const float DotProduct = FVector::DotProduct(Forward, ToSourceNormalized);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	DEBUG_LOG(TEXT("CheckParrySuccess: Angle=%.1f, ParryAngle=%.1f"), AngleDegrees, ParryAngle);

	if (AngleDegrees <= ParryAngle)
	{
		LastDefenseState = EPlayerDefenseState::Parried;
		DEBUG_LOG(TEXT("CheckParrySuccess: SUCCESS"));
	}
	else
	{
		DEBUG_LOG(TEXT("CheckParrySuccess: FAIL (angle exceeded)"));
	}
}

void UActionPracticeAbilitySystemComponent::ForceEnemyGroggy(AActor* EnemyActor)
{
	if (!EnemyActor)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EnemyActor);
	UEnemyAbilitySystemComponent* EnemyASC = Cast<UEnemyAbilitySystemComponent>(TargetASC);
	if (EnemyASC)
	{
		EnemyASC->ForceActivateGroggy();
		DEBUG_LOG(TEXT("ForceEnemyGroggy: Triggered groggy on %s"), *EnemyActor->GetName());
	}
}

void UActionPracticeAbilitySystemComponent::CheckBlockSuccess(AActor* SourceActor)
{
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
		//CalculateAndSetAttributes에서 최종 상태(Blocked/GuardBroken/ParryFallback*) 결정
		LastDefenseState = EPlayerDefenseState::Blocked;
	}
}

void UActionPracticeAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}