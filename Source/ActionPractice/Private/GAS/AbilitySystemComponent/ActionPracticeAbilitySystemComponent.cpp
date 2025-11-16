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
		CachedAPCharacter = Cast<AActionPracticeCharacter>(Owner);
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

	CachedAPCharacter = Cast<AActionPracticeCharacter>(InOwnerActor);
}

const UActionPracticeAttributeSet* UActionPracticeAbilitySystemComponent::GetActionPracticeAttributeSet() const 
{
	return this->GetSet<UActionPracticeAttributeSet>();
}

void UActionPracticeAbilitySystemComponent::CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData)
{
	CheckBlockSuccess(SourceActor);

	if (!CachedAPCharacter.IsValid())
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
		AWeapon* LeftWeapon = CachedAPCharacter->GetLeftWeapon();

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

	if (!CachedAPCharacter.IsValid() || !SourceActor)
	{
		return;
	}

	//방어 태그 확인
	const bool bIsBlocking = HasMatchingGameplayTag(StateAbilityBlockingTag);

	if (!bIsBlocking || !CachedAPCharacter->GetLeftWeapon())
	{
		return;
	}

	//공격자 방향 계산
	const FVector ToSource = SourceActor->GetActorLocation() - CachedAPCharacter->GetActorLocation();
	const FVector ToSourceNormalized = ToSource.GetSafeNormal2D();
	const FVector Forward = CachedAPCharacter->GetActorForwardVector();

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