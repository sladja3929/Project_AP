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
	DEFINE_LOG_CATEGORY_STATIC(LogBaseAbilitySystemComponent, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseAbilitySystemComponent, Warning, Format, ##__VA_ARGS__)
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
}

void UActionPracticeAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	OwnerCharacter = Cast<AActionPracticeCharacter>(InOwnerActor);
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

	//방어성공 시 계산
	if (bBlockedLastAttack)
	{
		AWeapon* LeftWeapon = OwnerCharacter->GetLeftWeapon();

		//무기의 DamageReduction 적용
		const FBlockActionData* BlockData = LeftWeapon->GetWeaponBlockData();
		const float DamageReduction = BlockData ? BlockData->DamageReduction : 0.0f;
		const float FinalDamage = FinalAttackData.FinalDamage * (1.0f - DamageReduction / 100.0f);

		//HP 적용
		const float OldHealth = APAttributeSet->GetHealth();
		APAttributeSet->SetHealth(FMath::Clamp(OldHealth - FinalDamage, 0.0f, APAttributeSet->GetMaxHealth()));
		
		//포이즈 대미지 적용
		if (FinalAttackData.PoiseDamage > 0.0f)
		{
			const float OldPoise = APAttributeSet->GetPoise();
			APAttributeSet->SetPoise(FMath::Clamp(OldPoise - FinalAttackData.PoiseDamage, 0.0f, APAttributeSet->GetMaxPoise()));
		}

		DEBUG_LOG(TEXT("Blocked: Damage=%.1f, FinalDamage=%.1f, DamageReduction=%.1f%%, Health=%.1f/%.1f"),
			FinalAttackData.FinalDamage, FinalDamage, DamageReduction,
			APAttributeSet->GetHealth(), APAttributeSet->GetMaxHealth());
		return;
	}

	//기본 or 방어실패: 기본 피격 계산식 사용
	Super::CalculateAndSetAttributes(SourceActor, FinalAttackData);
}

void UActionPracticeAbilitySystemComponent::PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData)
{
	Super::PrepareHitReactionEventData(OutEventData, FinalAttackData);

	//블로킹 상태를 TargetTags에 추가
	if (bBlockedLastAttack)
	{
		OutEventData.TargetTags.AddTag(StateAbilityBlockingTag);
		DEBUG_LOG(TEXT("Block Reaction triggered"));
	}
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