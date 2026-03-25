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

	//Impact Cue 태그 초기화
	GameplayCueImpactTag = UGameplayTagsSubsystem::GetGameplayCueImpactTag();
}

void UBaseAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{	
	Super::EndPlay(EndPlayReason);
}

void UBaseAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	CachedCharacter = Cast<ABaseCharacter>(InOwnerActor);

	//죽음 가드 리셋 (캐릭터가 다시 살아날 때 재사용 가능)
	bDeathHandled = false;

	//AttributeSet의 OnDamagedPreResolve 델리게이트 바인딩
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	if (UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet))
	{
		BaseAttributeSet->OnDamagedPreResolve.AddUObject(this, &UBaseAbilitySystemComponent::OnDamaged);
	}

	//외부 바인딩용 신호
	OnASCInitialized.Broadcast(this);
}

bool UBaseAbilitySystemComponent::TryActivateAbilityWithEventData(FGameplayAbilitySpecHandle AbilityToActivate, const FGameplayEventData* TriggerEventData)
{
	return InternalTryActivateAbility(AbilityToActivate, FPredictionKey(), nullptr, nullptr, TriggerEventData);
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
	const FFinalAttackData& FinalAttackData,
	const FHitResult* HitResult)
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

	//ActionPracticeGameplayEffectContext 추출하여 DamageType, PoiseDamage, HitResult 설정
	FGameplayEffectContext* Context = SpecHandle.Data.Get()->GetContext().Get();
	FActionPracticeGameplayEffectContext* APContext = static_cast<FActionPracticeGameplayEffectContext*>(Context);

	if (APContext)
	{
		APContext->SetAttackDamageType(FinalAttackData.DamageType);
		APContext->SetPoiseDamage(FinalAttackData.PoiseDamage);
		APContext->SetUnparriable(FinalAttackData.bUnparriable);

		//HitResult를 Context에 추가 — GAS가 Cue 파라미터에 Location/Normal을 자동 매핑
		if (HitResult)
		{
			APContext->AddHitResult(*HitResult, true);
		}
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

void UBaseAbilitySystemComponent::HandleDeath()
{
	if (bDeathHandled) return;
	bDeathHandled = true;

	//StateDead 태그는 PlayerDeathAbility가 몽타주 종료 후 직접 추가
	//UI가 몽타주 연출 이후에 반응하도록 타이밍 분리
	DEBUG_LOG(TEXT("HandleDeath: bDeathHandled set"));
}

void UBaseAbilitySystemComponent::ResetDeathHandled()
{
	bDeathHandled = false;
	DEBUG_LOG(TEXT("ResetDeathHandled: death guard cleared"));
}

void UBaseAbilitySystemComponent::CacheDamageEffectContext(const FGameplayEffectContextHandle& InContext)
{
	CachedDamageContext = InContext;
}

void UBaseAbilitySystemComponent::OnDamaged(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	//1단계: 어트리뷰트에 대미지 적용 (HP, Poise 등 — 음수 허용)
	CalculateAndSetAttributes(SourceActor, FinalAttackData);

	//2단계: 판정 결과 확정 후 Impact Cue 수동 발동
	ExecuteImpactCue(FinalAttackData);

	//3단계: 피격 반응 판정 및 실행 (사망 > 그로기 > 히트리액션)
	HandleOnDamagedResolved(SourceActor, FinalAttackData);

	//4단계: 브레이크 게이지 리셋 (사용 완료된 음수 게이지를 최대치로 복원)
	ResetBreakGauges();
}

EDefenseResult UBaseAbilitySystemComponent::GetDefenseResult() const
{
	return EDefenseResult::None;
}

void UBaseAbilitySystemComponent::ExecuteImpactCue(const FFinalAttackData& FinalAttackData)
{
	if (!CachedDamageContext.IsValid())
	{
		DEBUG_LOG(TEXT("ExecuteImpactCue: No cached context"));
		return;
	}

	if (!GameplayCueImpactTag.IsValid())
	{
		DEBUG_LOG(TEXT("ExecuteImpactCue: ImpactCueTag is not valid"));
		CachedDamageContext.Clear();
		return;
	}

	//원본 Context를 복제하여 DefenseResult 주입 (원본 오염 방지)
	FGameplayEffectContextHandle CueContext(CachedDamageContext.Get()->Duplicate());
	FActionPracticeGameplayEffectContext* APContext = static_cast<FActionPracticeGameplayEffectContext*>(CueContext.Get());
	if (APContext)
	{
		APContext->SetDefenseResult(GetDefenseResult());
	}

	FGameplayCueParameters CueParams;
	CueParams.EffectContext = CueContext;

	ExecuteGameplayCue(GameplayCueImpactTag, CueParams);

	CachedDamageContext.Clear();
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

	//포이즈 대미지 적용 (하한 클램핑 없음 — 음수값이 HitReaction 강도 판정에 사용됨)
	if (FinalAttackData.PoiseDamage > 0.0f)
	{
		const float OldPoise = BaseAttributeSet->GetPoise();
		BaseAttributeSet->SetPoise(OldPoise - FinalAttackData.PoiseDamage);
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
		HandleDeath();
		return;
	}

	//HitReaction 활성화 여부 체크
	if (ShouldActivateHitReaction())
	{
		if (AbilityHitReactionTag.IsValid())
		{
			TArray<FGameplayAbilitySpec*> HitReactionSpecs;
			GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityHitReactionTag), HitReactionSpecs);

			if (HitReactionSpecs.Num() > 0)
			{
				FGameplayEventData EventData;
				PrepareHitReactionEventData(EventData, FinalAttackData);

				if (TryActivateAbilityWithEventData(HitReactionSpecs[0]->Handle, &EventData))
				{
					DEBUG_LOG(TEXT("HitReaction activated with Poise=%.1f"), EventData.EventMagnitude);
				}
				else
				{
					DEBUG_LOG(TEXT("HitReaction activation failed"));
				}
			}
			else
			{
				DEBUG_LOG(TEXT("HitReaction Ability not found"));
			}
		}
	}
}

bool UBaseAbilitySystemComponent::ShouldActivateHitReaction() const
{
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	const UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (!BaseAttributeSet)
	{
		return false;
	}

	//포이즈 브레이크 체크
	return BaseAttributeSet->GetPoise() <= 0.0f;
}

void UBaseAbilitySystemComponent::PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData)
{
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (BaseAttributeSet)
	{
		OutEventData.EventMagnitude = BaseAttributeSet->GetPoise(); //음수값
		OutEventData.EventTag = AbilityHitReactionTag;
	}
}

void UBaseAbilitySystemComponent::ResetBreakGauges()
{
	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(UBaseAttributeSet::StaticClass()));
	UBaseAttributeSet* BaseAttributeSet = Cast<UBaseAttributeSet>(AttributeSet);
	if (!BaseAttributeSet)
	{
		return;
	}

	if (BaseAttributeSet->GetPoise() <= 0.0f)
	{
		BaseAttributeSet->SetPoise(BaseAttributeSet->GetMaxPoise());
		DEBUG_LOG(TEXT("ResetBreakGauges: Poise reset to %.1f"), BaseAttributeSet->GetMaxPoise());
	}
}