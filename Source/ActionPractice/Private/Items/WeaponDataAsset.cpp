#include "Items/WeaponDataAsset.h"
#include "Engine/AssetManager.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWeaponDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWeaponDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UWeaponDataAsset::PreloadAllMontages()
{
	//중복 요청 방지 — 번들 핸들이 로드된 몽타주를 계속 붙잡고 있으므로 한 번만 요청하면 된다
	//AWeapon::BeginPlay와 UAttackSequenceAbility::CacheWeaponData 양쪽에서 호출될 수 있다
	if (bPreloadRequested || !UAssetManager::IsInitialized())
	{
		return;
	}

	const FPrimaryAssetId AssetId = GetPrimaryAssetId();
	if (!AssetId.IsValid())
	{
		//AssetManager 스캔 미등록/오설정 시 PrimaryAssetId가 Invalid → 번들 로드 no-op
		DEBUG_LOG(TEXT("PreloadAllMontages: invalid PrimaryAssetId — check PrimaryAssetTypesToScan(WeaponData) settings"));
		return;
	}

	bPreloadRequested = true;

	//Combat 번들(콤보 AttackMontage + Charge SubAttackMontage + 블록/패리 몽타주 전부)을 번들 단위로 로드
	//meta=(AssetBundles="Combat") + PrimaryAssetTypesToScan 등록으로 쿡 시 번들 데이터가 생성된다
	//반환 핸들을 멤버로 보관해 AssetManager 참조 유지 시맨틱에 의존하지 않고 참조를 확정적으로 붙잡는다
	//AttackSequenceAbility 등이 사용 시점에 같은 몽타주를 중복 요청(LoadSynchronous)해도 참조 카운팅으로 안전하다
	BundleHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
		{ AssetId },
		{ FName(TEXT("Combat")) },
		{},
		false,
		FStreamableDelegate::CreateWeakLambda(this, []()
		{
			DEBUG_LOG(TEXT("WeaponDataAsset Combat bundle load complete"));
		}),
		FStreamableManager::DefaultAsyncLoadPriority);
}

void UWeaponDataAsset::BeginDestroy()
{
	//Combat 번들 핸들 정리 (핸들이 잡고 있던 몽타주 레퍼런스 해제)
	if (BundleHandle.IsValid())
	{
		BundleHandle->CancelHandle();
		BundleHandle.Reset();
	}

	Super::BeginDestroy();
}
