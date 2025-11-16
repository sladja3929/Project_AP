#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Characters/BaseCharacter.h"
#include "GAS/Effects/ActionPracticeGameplayEffectContext.h"
#include "Items/AttackData.h"
#include "GameplayEffect.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBaseAbilitySystemComponent, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseAbilitySystemComponent, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UBaseAbilitySystemComponent::UBaseAbilitySystemComponent()
{
	SetIsReplicated(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UBaseAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CachedCharacter = Cast<ABaseCharacter>(Owner);
	}

	//HitReaction 태그 초기화
	AbilityHitReactionTag = UGameplayTagsSubsystem::GetAbilityHitReactionTag();
}

void UBaseAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UBaseAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	CachedCharacter = Cast<ABaseCharacter>(InOwnerActor);

	//AttributeSet의 OnDamagedPreResolve 델리게이트 바인딩
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	if (UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet))
	{
		BaseAttributeSet->OnDamagedPreResolve.AddUObject(this, &UBaseAbilitySystemComponent::OnDamaged);
	}

	//외부 바인딩용 신호
	OnASCInitialized.Broadcast(this);
}

FGameplayEffectSpecHandle UBaseAbilitySystemComponent::CreateGameplayEffectSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, UObject* SourceObject)
{
	if (!GameplayEffectClass)
	{
		DEBUG_LOG(TEXT("GameplayEffectClass is null"));
		return FGameplayEffectSpecHandle();
	}

	//ActionPracticeAbilitySystemGlobals에 의해 자동으로 ActionPracticeGameplayEffectContext 생성
	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	if (SourceObject)
	{
		EffectContext.AddSourceObject(SourceObject);
	}

	//Spec 생성
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);

	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("Failed to create GameplayEffectSpec"));
		return FGameplayEffectSpecHandle();
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UBaseAbilitySystemComponent::CreateAttackGameplayEffectSpec(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	float Level,
	UObject* SourceObject,
	const FFinalAttackData& FinalAttackData)
{
	//기본 Spec 생성
	FGameplayEffectSpecHandle SpecHandle = CreateGameplayEffectSpec(GameplayEffectClass, Level, SourceObject);

	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("Failed to create Attack GameplayEffectSpec"));
		return FGameplayEffectSpecHandle();
	}

	//Incoming Damage Attribute Magnitude 설정
	SetSpecSetByCallerMagnitude(SpecHandle, UGameplayTagsSubsystem::GetEffectDamageIncomingDamageTag(), FinalAttackData.FinalDamage);

	//ActionPracticeGameplayEffectContext 추출하여 DamageType, PoiseDamage 설정
	FGameplayEffectContext* Context = SpecHandle.Data.Get()->GetContext().Get();
	FActionPracticeGameplayEffectContext* APContext = static_cast<FActionPracticeGameplayEffectContext*>(Context);

	if (APContext)
	{
		APContext->SetAttackDamageType(FinalAttackData.DamageType);
		APContext->SetPoiseDamage(FinalAttackData.PoiseDamage);
	}
	else
	{
		DEBUG_LOG(TEXT("Failed to cast to FActionPracticeGameplayEffectContext"));
	}

	return SpecHandle;
}

void UBaseAbilitySystemComponent::SetSpecSetByCallerMagnitude(FGameplayEffectSpecHandle& SpecHandle, const FGameplayTag& Tag, float Magnitude)
{
	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("Invalid SpecHandle"));
		return;
	}

	if (!Tag.IsValid())
	{
		DEBUG_LOG(TEXT("Invalid Tag"));
		return;
	}

	SpecHandle.Data.Get()->SetSetByCallerMagnitude(Tag, Magnitude);
}

void UBaseAbilitySystemComponent::SetSpecSetByCallerMagnitudes(FGameplayEffectSpecHandle& SpecHandle, const TMap<FGameplayTag, float>& Magnitudes)
{
	if (!SpecHandle.IsValid())
	{
		DEBUG_LOG(TEXT("Invalid SpecHandle"));
		return;
	}

	for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
	{
		if (Pair.Key.IsValid())
		{
			SpecHandle.Data.Get()->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		}
	}
}

void UBaseAbilitySystemComponent::OnDamaged(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	//방어력 계산 및 Attribute 설정
	CalculateAndSetAttributes(SourceActor, FinalAttackData);

	//피격 로직 트리거
	HandleOnDamagedResolved(SourceActor, FinalAttackData);
}

void UBaseAbilitySystemComponent::CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	if (!CachedCharacter.IsValid())
	{
		return;
	}

	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (!BaseAttributeSet)
	{
		return;
	}

	//방어력 계산
	const float Defense = BaseAttributeSet->GetDefense();
	const float DefenseReduction = Defense / (Defense + 100.0f);
	const float FinalDamage = FinalAttackData.FinalDamage * (1.0f - DefenseReduction);

	//HP 적용
	const float OldHealth = BaseAttributeSet->GetHealth();
	BaseAttributeSet->SetHealth(FMath::Clamp(OldHealth - FinalDamage, 0.0f, BaseAttributeSet->GetMaxHealth()));

	//포이즈 대미지 적용
	if (FinalAttackData.PoiseDamage > 0.0f)
	{
		const float OldPoise = BaseAttributeSet->GetPoise();
		BaseAttributeSet->SetPoise(FMath::Clamp(OldPoise - FinalAttackData.PoiseDamage, 0.0f, BaseAttributeSet->GetMaxPoise()));
	}

	DEBUG_LOG(TEXT("OnDamaged: Damage=%.1f, FinalDamage=%.1f, Health=%.1f/%.1f, Poise=%.1f/%.1f"),
		FinalAttackData.FinalDamage, FinalDamage,
		BaseAttributeSet->GetHealth(), BaseAttributeSet->GetMaxHealth(),
		BaseAttributeSet->GetPoise(), BaseAttributeSet->GetMaxPoise());
}

void UBaseAbilitySystemComponent::HandleOnDamagedResolved(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	if (!CachedCharacter.IsValid())
	{
		return;
	}

	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (!BaseAttributeSet)
	{
		return;
	}

	//죽음 체크
	if (BaseAttributeSet->GetHealth() <= 0.0f)
	{
		DEBUG_LOG(TEXT("HandleOnDamagedResolved: Character died"));
		//TODO: 죽음 처리
		return;
	}

	//포이즈 브레이크 체크
	if (BaseAttributeSet->GetPoise() <= 0.0f)
	{
		DEBUG_LOG(TEXT("HandleOnDamagedResolved: Poise broken, Poise=%.1f"), BaseAttributeSet->GetPoise());

		//HitReaction Ability 활성화
		if (AbilityHitReactionTag.IsValid())
		{
			TArray<FGameplayAbilitySpec*> HitReactionSpecs;
			GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityHitReactionTag), HitReactionSpecs);

			if (HitReactionSpecs.Num() > 0)
			{
				FGameplayAbilitySpec* HitReactionSpec = HitReactionSpecs[0];

				//EventData 준비
				FGameplayEventData EventData;
				PrepareHitReactionEventData(EventData, FinalAttackData);

				FGameplayAbilityActorInfo* ActorInfo = const_cast<FGameplayAbilityActorInfo*>(AbilityActorInfo.Get());
				if (ActorInfo && HitReactionSpec)
				{
					TriggerAbilityFromGameplayEvent(
						HitReactionSpec->Handle,
						ActorInfo,
						AbilityHitReactionTag,
						&EventData,
						*this
					);
					DEBUG_LOG(TEXT("HitReaction Ability activated with Poise=%.1f"), EventData.EventMagnitude);
				}
			}
			else
			{
				DEBUG_LOG(TEXT("HitReaction Ability not found"));
			}
		}
	}
}

void UBaseAbilitySystemComponent::PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData)
{
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (BaseAttributeSet)
	{
		OutEventData.EventMagnitude = BaseAttributeSet->GetPoise(); // 음수값
		OutEventData.EventTag = AbilityHitReactionTag;
	}
}