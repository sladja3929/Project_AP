#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionPracticeLandscapeEditLibrary.generated.h"

class ALandscape;
class UMaterialInterface;

//에디터 Python/MCP 툴세트에서 호출하는 랜드스케이프 편집 함수 모음
//새 랜드스케이프 생성(ALandscapeProxy::Import)은 C++ 전용이라 BlueprintCallable로 감싸 unreal.ActionPracticeLandscapeEditLibrary로 노출
UCLASS()
class ACTIONPRACTICEEDITOR_API UActionPracticeLandscapeEditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	//에디터 월드의 현재 레벨에 높이맵·웨이트맵 raw 파일로 새 랜드스케이프 생성
	//크기는 (ComponentCount * SectionsPerComponent * QuadsPerSection + 1) 정점, Location은 정점 (0,0)의 월드 위치
	//HeightmapFile: 16비트 리틀엔디언 raw(.r16), 32768이 Z 0, 한 단위 = Scale.Z / 128 cm
	//WeightmapFiles: LayerNames와 같은 순서의 8비트 raw, 정점마다 레이어 합이 255가 되도록 정규화된 데이터(Additive)
	//레이어 인포 에셋은 LayerInfoFolder/<LayerName>_LayerInfo를 재사용하거나 새로 생성 (저장은 호출 측)
	//실패 시 에러 메시지 반환, 성공 시 빈 문자열
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Landscape")
	static FString CreateLandscapeFromRaw(FVector Location, FVector Scale, int32 ComponentCountX, int32 ComponentCountY, int32 SectionsPerComponent, int32 QuadsPerSection,
		const FString& HeightmapFile, const TArray<FName>& LayerNames, const TArray<FString>& WeightmapFiles, UMaterialInterface* Material, const FString& LayerInfoFolder,
		ALandscape*& OutLandscape);

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
