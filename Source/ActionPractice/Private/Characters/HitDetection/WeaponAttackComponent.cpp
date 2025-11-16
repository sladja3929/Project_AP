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

	//WeaponData의 모든 소켓 정보를 미리 빌드
	const UWeaponDataAsset* WeaponData = OwnerWeapon->GetWeaponData();
	if (WeaponData)
	{
		BuildSocketConfigs(WeaponData->HitSocketInfo);
	}

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
	DEBUG_LOG(TEXT("LoadTraceConfig - START, ComboIndex: %d"), ComboIndex);

	if (!OwnerWeapon)
	{
		DEBUG_LOG(TEXT("LoadTraceConfig - FAILED: No OwnerWeapon"));
		return false;
	}

	const UWeaponDataAsset* WeaponData = OwnerWeapon->GetWeaponData();
	if (!WeaponData)
	{
		DEBUG_LOG(TEXT("LoadTraceConfig - FAILED: No WeaponData"));
		return false;
	}

	//무기 데이터 가져오기
	const FTaggedAttackData* AttackData = OwnerWeapon->GetWeaponAttackDataByTag(AttackTags);
	if (!AttackData || AttackData->ComboSequence.Num() == 0)
	{
		DEBUG_LOG(TEXT("LoadTraceConfig - FAILED: No AttackData or empty ComboSequence"));
		return false;
	}

	//콤보 인덱스 유효성 검사
	ComboIndex = FMath::Clamp(ComboIndex, 0, AttackData->ComboSequence.Num() - 1);
	const FAttackStats& AttackInfo = AttackData->ComboSequence[ComboIndex].AttackData;

	UsingHitSocketGroups.Empty();

	//AttackStats의 UsingSocketConfigs에서 사용할 소켓들을 가져옴
	for (const FAttackSocketConfig& SocketConfig : AttackInfo.UsingSocketConfigs)
	{
		if (FHitSocketGroupConfig* PrebuiltSocketGroup = PrebuiltSocketGroups.Find(SocketConfig.SocketName))
		{
			//미리 빌드된 설정을 복사하고 DamageType과 TraceRadius 설정
			FHitSocketGroupConfig SocketGroupConfig = *PrebuiltSocketGroup;
			SocketGroupConfig.AttackMotionType = AttackInfo.DamageType;
			SocketGroupConfig.TraceRadius = SocketConfig.TraceRadius;

			UsingHitSocketGroups.Add(SocketConfig.SocketName, SocketGroupConfig);
			DEBUG_LOG(TEXT("LoadTraceConfig - Added socket group: %s, SocketCount: %d, Radius: %.2f"),
				*SocketConfig.SocketName.ToString(), SocketGroupConfig.SocketCount, SocketGroupConfig.TraceRadius);
		}
		else
		{
			DEBUG_LOG(TEXT("LoadTraceConfig - Socket not found in PrebuiltSocketGroups: %s"), *SocketConfig.SocketName.ToString());
		}
	}

	//공격 데이터 설정
	CurrentAttackData.FinalDamage = OwnerWeapon->GetCalculatedDamage() * AttackInfo.DamageMultiplier;
	CurrentAttackData.PoiseDamage = AttackInfo.PoiseDamage;
	CurrentAttackData.DamageType = AttackInfo.DamageType;

	DEBUG_LOG(TEXT("LoadTraceConfig - SUCCESS: Added %d socket groups, FinalDamage: %.2f"),
		UsingHitSocketGroups.Num(), CurrentAttackData.FinalDamage);

	return true;
}

void UWeaponAttackComponent::SetOwnerMesh()
{
	OwnerMesh = OwnerWeapon->FindComponentByClass<UStaticMeshComponent>();

	if (!OwnerMesh)
	{
		DEBUG_LOG(TEXT("WeaponCollisionComponent: No Weapon StaticMesh"));
		return;
	}
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