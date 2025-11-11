// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/HitDetection/EnemyAttackComponent.h"
#include "Characters/BossCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "Items/AttackData.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/InputComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyAttackComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyAttackComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UEnemyAttackComponent::UEnemyAttackComponent()
{
}

void UEnemyAttackComponent::BeginPlay()
{
	OwnerEnemy = Cast<ABossCharacter>(GetOwner());
	if (!OwnerEnemy)
	{
		DEBUG_LOG(TEXT("EnemyAttackComponent: Owner is not an Enemy!"));
		return;
	}

	Super::BeginPlay();

	//EnemyData의 모든 소켓 정보를 미리 빌드
	const UEnemyDataAsset* EnemyData = OwnerEnemy->GetEnemyData();
	if (EnemyData)
	{
		BuildSocketConfigs(EnemyData->HitSocketInfo);
	}

	//디버그용 2번 키 바인딩 (1번은 Weapon이 사용)
	if (GetWorld())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->InputComponent)
			{
				PC->InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &UEnemyAttackComponent::ToggleWeaponDebugTrace);
			}
		}
	}
}

AActor* UEnemyAttackComponent::GetOwnerActor() const
{
	return OwnerEnemy;
}

UAbilitySystemComponent* UEnemyAttackComponent::GetOwnerASC() const
{
	if (!OwnerEnemy) return nullptr;

	return OwnerEnemy->GetAbilitySystemComponent();
}

#pragma region "Trace Config Functions"
bool UEnemyAttackComponent::LoadTraceConfig(const FName& AttackName, int32 ComboIndex)
{
	DEBUG_LOG(TEXT("LoadTraceConfig - START, AttackName: %s, ComboIndex: %d"), *AttackName.ToString(), ComboIndex);

	if (!OwnerEnemy)
	{
		DEBUG_LOG(TEXT("LoadTraceConfig - FAILED: No OwnerEnemy"));
		return false;
	}

	//Enemy 정보 가져오기
	const UEnemyDataAsset* EnemyData = OwnerEnemy->GetEnemyData();
	if (!EnemyData)
	{
		DEBUG_LOG(TEXT("LoadTraceConfig - FAILED: No EnemyData"));
		return false;
	}

	const FNamedAttackData* AttackData = EnemyData->NamedAttackData.Find(AttackName);
	if (!AttackData || AttackData->AttackStats.Num() == 0)
	{
		DEBUG_LOG(TEXT("LoadTraceConfig - FAILED: No AttackData or empty AttackStats"));
		return false;
	}

	//콤보 인덱스 유효성 검사
	ComboIndex = FMath::Clamp(ComboIndex, 0, AttackData->AttackStats.Num() - 1);
	const FAttackStats& AttackInfo = AttackData->AttackStats[ComboIndex];

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
	CurrentAttackData.FinalDamage = EnemyData->BaseDamage * AttackInfo.DamageMultiplier;
	CurrentAttackData.PoiseDamage = AttackInfo.PoiseDamage;
	CurrentAttackData.DamageType = AttackInfo.DamageType;

	DEBUG_LOG(TEXT("LoadTraceConfig - SUCCESS: Added %d socket groups, FinalDamage: %.2f"),
		UsingHitSocketGroups.Num(), CurrentAttackData.FinalDamage);

	return true;
}

void UEnemyAttackComponent::SetOwnerMesh()
{
	if (!OwnerEnemy) return;

	//SkeletalMeshComponent 가져오기
	OwnerMesh = OwnerEnemy->GetMesh();

	if (!OwnerMesh)
	{
		DEBUG_LOG(TEXT("EnemyAttackComponent: No Enemy SkeletalMesh"));
		return;
	}
}
#pragma endregion

#pragma region "Hit Functions"
void UEnemyAttackComponent::AddIgnoredActors(FCollisionQueryParams& Params) const
{
	if (OwnerEnemy)
	{
		Params.AddIgnoredActor(OwnerEnemy);
		// TODO: 필요시 아군 Enemy 제외 로직 추가
	}
}
#pragma endregion