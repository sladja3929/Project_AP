#include "Items/UsableItemDataAsset.h"
#include "Engine/AssetManager.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogUsableItemDataAsset, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogUsableItemDataAsset, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UUsableItemDataAsset::PreloadAssets()
{
	//중복 요청 방지 — 번들/메시 핸들이 로드된 에셋을 계속 붙잡고 있으므로 한 번만 요청하면 된다
	if (bPreloadRequested || !UAssetManager::IsInitialized())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();

	//1) UseMontage — Combat 번들로 로드 (노티파이 타임라인/길이 기반 어빌리티 종료에 필요, 서버에서도 로드)
	//meta=(AssetBundles="Combat") + PrimaryAssetTypesToScan(UsableItem) 등록으로 쿡 시 번들 데이터가 생성된다
	if (!UseMontage.IsNull())
	{
		const FPrimaryAssetId AssetId = GetPrimaryAssetId();
		if (AssetId.IsValid())
		{
			//반환 핸들을 멤버로 보관해 AssetManager 참조 유지 시맨틱에 의존하지 않고 몽타주 참조를 확정적으로 붙잡는다
			BundleHandle = AssetManager.ChangeBundleStateForPrimaryAssets(
				{ AssetId },
				{ FName(TEXT("Combat")) },
				{},
				false,
				FStreamableDelegate::CreateWeakLambda(this, []()
				{
					DEBUG_LOG(TEXT("UsableItemDataAsset Combat bundle load complete"));
				}),
				FStreamableManager::DefaultAsyncLoadPriority);
		}
		else
		{
			DEBUG_LOG(TEXT("PreloadAssets: invalid PrimaryAssetId — check PrimaryAssetTypesToScan(UsableItem) settings"));
		}
	}

	//2) UseMesh(StaticMesh) — Visual 전용이므로 Combat 번들이 아닌 기존 RequestAsyncLoad 유지
	//순수 시각 표현(NoCollision 손 소품)이라 데디케이티드 서버에서는 스킵해 서버 메모리를 아낀다
	if (!IsRunningDedicatedServer() && !UseMesh.IsNull())
	{
		FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();
		PreloadHandle = StreamableManager.RequestAsyncLoad(
			UseMesh.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(this, []()
			{
				DEBUG_LOG(TEXT("UsableItemDataAsset UseMesh async preload complete"));
			}),
			FStreamableManager::DefaultAsyncLoadPriority,
			false,
			false,
			FString(TEXT("UUsableItemDataAsset UseMesh")));
	}

	//두 경로(번들/메시)를 모두 시도한 뒤 플래그 설정 — 유효성 확인 이후에 세팅하여 Weapon/Enemy DA와 순서 일관성 유지
	bPreloadRequested = true;
}

void UUsableItemDataAsset::BeginDestroy()
{
	//Combat 번들 핸들 정리 (UseMontage 참조 해제)
	if (BundleHandle.IsValid())
	{
		BundleHandle->CancelHandle();
		BundleHandle.Reset();
	}

	//UseMesh RequestAsyncLoad 핸들 정리 (메시 참조 해제)
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}

	Super::BeginDestroy();
}
