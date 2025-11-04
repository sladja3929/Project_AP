// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/HitDetection/WeaponAttackComponent.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Items/AttackData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/InputComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWeaponAttackTraceComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWeaponAttackTraceComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UWeaponAttackComponent::UWeaponAttackComponent()
{
}

void UWeaponAttackComponent::BeginPlay()
{
	OwnerWeapon = Cast<AWeapon>(GetOwner());
	if (!OwnerWeapon)
	{
		DEBUG_LOG(TEXT("WeaponCollisionComponent: Owner is not a weapon!"));
		return;
	}

	Super::BeginPlay();

	//디버그용 1번 키 바인딩
	if (GetWorld())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->InputComponent)
			{
				PC->InputComponent->BindKey(EKeys::One, IE_Pressed, this, &UWeaponAttackComponent::ToggleWeaponDebugTrace);
			}
		}
	}
}

AActor* UWeaponAttackComponent::GetOwnerActor() const
{
	return OwnerWeapon;
}

UAbilitySystemComponent* UWeaponAttackComponent::GetOwnerASC() const
{
	if (!OwnerWeapon) return nullptr;

	AActionPracticeCharacter* Character = OwnerWeapon->GetOwnerCharacter();
	return Character ? Character->GetAbilitySystemComponent() : nullptr;
}

#pragma region "Trace Config Functions"
bool UWeaponAttackComponent::LoadTraceConfig(const FGameplayTagContainer& AttackTags, int32 ComboIndex)
{
	if (!OwnerWeapon) return false;

	const UWeaponDataAsset* WeaponData = OwnerWeapon->GetWeaponData();
	if (!WeaponData) return false;

	//무기 데이터 가져오기
	const FTaggedAttackData* AttackData = OwnerWeapon->GetWeaponAttackDataByTag(AttackTags);
	if (!AttackData || AttackData->ComboSequence.Num() == 0) return false;

	//콤보 인덱스 유효성 검사
	ComboIndex = FMath::Clamp(ComboIndex, 0, AttackData->ComboSequence.Num() - 1);
	const FAttackStats& AttackInfo = AttackData->ComboSequence[ComboIndex].AttackData;

	//트레이스 설정
	CurrentTraceConfig.AttackMotionType = AttackInfo.DamageType;
	CurrentTraceConfig.SocketCount = WeaponData->HitSocketCount;
	CurrentTraceConfig.TraceRadius = WeaponData->HitRadius;

	//공격 데이터 설정
	CurrentAttackData.FinalDamage = OwnerWeapon->GetCalculatedDamage() * AttackInfo.DamageMultiplier;
	CurrentAttackData.PoiseDamage = AttackInfo.PoiseDamage;
	CurrentAttackData.DamageType = AttackInfo.DamageType;

	return true;
}

void UWeaponAttackComponent::SetOwnerMesh()
{
	OwnerMesh = OwnerWeapon->FindComponentByClass<UStaticMeshComponent>();
	//무기 끝 소켓까지 확인
	if (!OwnerMesh || !OwnerMesh->DoesSocketExist(FName(*FString::Printf(TEXT("trace_socket_0")))))
	{
		OwnerMesh = nullptr;
		DEBUG_LOG(TEXT("WeaponCollisionComponent: No Weapon StaticMesh or trace_socket_0"));
		return;
	}

	TipSocketName = FName(*FString::Printf(TEXT("%s0"), *SocketNamePrefix.ToString()));
}
#pragma endregion

#pragma region "Hit Functions"
void UWeaponAttackComponent::AddIgnoredActors(FCollisionQueryParams& Params) const
{
	if (OwnerWeapon)
	{
		Params.AddIgnoredActor(OwnerWeapon);

		if (AActionPracticeCharacter* Character = OwnerWeapon->GetOwnerCharacter())
		{
			Params.AddIgnoredActor(Character);
		}
	}
}
#pragma endregion