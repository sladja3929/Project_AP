#include "ActionPracticeLandscapeEditLibrary.h"

#include "Editor.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeLandscapeEditLibrary, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeLandscapeEditLibrary, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#define LOCTEXT_NAMESPACE "ActionPracticeLandscapeEditLibrary"

namespace
{
	//LayerInfoFolder/<LayerName>_LayerInfo 에셋 재사용, 없으면 생성
	ULandscapeLayerInfoObject* FindOrCreateLayerInfo(const FName& LayerName, const FString& LayerInfoFolder)
	{
		const FString AssetName = LayerName.ToString() + TEXT("_LayerInfo");
		const FString ObjectPath = LayerInfoFolder / AssetName + TEXT(".") + AssetName;

		if (ULandscapeLayerInfoObject* Existing = LoadObject<ULandscapeLayerInfoObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}

		return UE::Landscape::CreateTargetLayerInfo(LayerName, LayerInfoFolder, AssetName);
	}
}

FString UActionPracticeLandscapeEditLibrary::CreateLandscapeFromRaw(FVector Location, FVector Scale, int32 ComponentCountX, int32 ComponentCountY, int32 SectionsPerComponent, int32 QuadsPerSection,
	const FString& HeightmapFile, const TArray<FName>& LayerNames, const TArray<FString>& WeightmapFiles, UMaterialInterface* Material, const FString& LayerInfoFolder,
	ALandscape*& OutLandscape)
{
	OutLandscape = nullptr;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	if (!World)
	{
		return TEXT("No editor world is loaded");
	}

	if (GEditor->PlayWorld)
	{
		return TEXT("Stop the Play-In-Editor session before creating a landscape");
	}

	//엔진 New Landscape 패널이 허용하는 값만 사용
	if (ComponentCountX < 1 || ComponentCountY < 1 || (SectionsPerComponent != 1 && SectionsPerComponent != 2))
	{
		return TEXT("ComponentCount must be >= 1 and SectionsPerComponent must be 1 or 2");
	}

	if (QuadsPerSection != 7 && QuadsPerSection != 15 && QuadsPerSection != 31 && QuadsPerSection != 63 && QuadsPerSection != 127 && QuadsPerSection != 255)
	{
		return TEXT("QuadsPerSection must be one of 7, 15, 31, 63, 127, 255");
	}

	if (LayerNames.Num() != WeightmapFiles.Num())
	{
		return TEXT("LayerNames and WeightmapFiles must have the same length");
	}

	if (LayerNames.Num() > 0 && !FPackageName::IsValidLongPackageName(LayerInfoFolder / TEXT("Dummy")))
	{
		return FString::Printf(TEXT("LayerInfoFolder is not a valid content path: %s"), *LayerInfoFolder);
	}

	const int32 QuadsPerComponent = SectionsPerComponent * QuadsPerSection;
	const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
	const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;
	const int64 VertexCount = static_cast<int64>(SizeX) * SizeY;

	TArray<uint8> HeightBytes;

	if (!FFileHelper::LoadFileToArray(HeightBytes, *HeightmapFile))
	{
		return FString::Printf(TEXT("Cannot read heightmap file: %s"), *HeightmapFile);
	}

	if (HeightBytes.Num() != VertexCount * 2)
	{
		return FString::Printf(TEXT("Heightmap is %d bytes, expected %lld (%dx%d uint16)"), HeightBytes.Num(), VertexCount * 2, SizeX, SizeY);
	}

	TArray<uint16> HeightData;
	HeightData.SetNumUninitialized(VertexCount);
	FMemory::Memcpy(HeightData.GetData(), HeightBytes.GetData(), HeightBytes.Num());

	TArray<FLandscapeImportLayerInfo> ImportLayers;

	for (int32 Index = 0; Index < LayerNames.Num(); ++Index)
	{
		FLandscapeImportLayerInfo& ImportLayer = ImportLayers.Emplace_GetRef(LayerNames[Index]);

		if (!FFileHelper::LoadFileToArray(ImportLayer.LayerData, *WeightmapFiles[Index]))
		{
			return FString::Printf(TEXT("Cannot read weightmap file: %s"), *WeightmapFiles[Index]);
		}

		if (ImportLayer.LayerData.Num() != VertexCount)
		{
			return FString::Printf(TEXT("Weightmap %s is %d bytes, expected %lld"), *WeightmapFiles[Index], ImportLayer.LayerData.Num(), VertexCount);
		}
	}

	//레이어 인포 에셋은 모든 파일 검증이 끝난 뒤 생성 (검증 실패 시 고아 에셋 방지)
	for (FLandscapeImportLayerInfo& ImportLayer : ImportLayers)
	{
		ImportLayer.LayerInfo = FindOrCreateLayerInfo(ImportLayer.LayerName, LayerInfoFolder);

		if (!ImportLayer.LayerInfo)
		{
			return FString::Printf(TEXT("Cannot create layer info for %s in %s"), *ImportLayer.LayerName.ToString(), *LayerInfoFolder);
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateLandscapeFromRaw", "Create Landscape From Raw"));

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);

	if (!Landscape)
	{
		return TEXT("Failed to spawn ALandscape");
	}

	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);

	//엔진 New Landscape 패널과 같은 라이트매스 LOD 계산
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo(static_cast<uint32>((SizeX * SizeY) / (2048 * 2048) + 1)), (uint32)2);

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));

	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), ImportLayers);

	Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, SectionsPerComponent, QuadsPerSection, HeightDataPerLayers, nullptr,
		MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();

	if (!LandscapeInfo)
	{
		//반쯤 생성된 액터를 남기지 않도록 정리
		Landscape->Destroy();
		return TEXT("Landscape import did not create a LandscapeInfo");
	}

	//Import 흐름에 의존하지 않고 컴포넌트 머티리얼 인스턴스를 명시적으로 갱신
	Landscape->UpdateAllComponentMaterialInstances();

	Landscape->SetActorLabel(TEXT("Landscape"));
	LandscapeInfo->UpdateLayerInfoMap(Landscape);

	//Import는 레이어 데이터만 기록하므로 타깃 레이어와 레이어 인포 연결은 별도 등록 (엔진 New Landscape 흐름과 동일)
	for (const FLandscapeImportLayerInfo& ImportLayer : ImportLayers)
	{
		Landscape->AddTargetLayer(ImportLayer.LayerName, FLandscapeTargetLayerSettings(ImportLayer.LayerInfo));

		const int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(ImportLayer.LayerName);

		if (LayerInfoIndex != INDEX_NONE)
		{
			LandscapeInfo->Layers[LayerInfoIndex].LayerInfoObj = ImportLayer.LayerInfo;
		}
	}

	OutLandscape = Landscape;

	DEBUG_LOG(TEXT("CreateLandscapeFromRaw: %dx%d vertices, %d layers, material %s"), SizeX, SizeY, ImportLayers.Num(), *GetNameSafe(Material));
	return FString();
}

#undef LOCTEXT_NAMESPACE
