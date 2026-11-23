#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionPracticeEditorScriptLibrary.generated.h"

class UStateTree;
class UAnimMontage;
class UAnimSequenceBase;
class UBlendSpace;
class USkeleton;
class UFontFace;

//에디터 Python/MCP 툴세트에서 호출하기 위한 스크립트 노출 함수 모음
//엔진이 C++로만 제공하는 에디터 기능을 BlueprintCallable로 감싸 unreal.ActionPracticeEditorScriptLibrary로 노출
UCLASS()
class ACTIONPRACTICEEDITOR_API UActionPracticeEditorScriptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	//StateTree 에셋 컴파일 (에디터 데이터 → 런타임 데이터). 컴파일러 로그 메시지 반환
	//bool 반환 + out 파라미터 조합은 Python에서 false 시 None으로 축약되어 메시지가 유실되므로 메시지를 반환값으로 사용
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static TArray<FString> CompileStateTree(UStateTree* StateTree, bool& bOutSucceeded);

	//에디터 데이터가 마지막 컴파일 이후 변경되어 재컴파일이 필요한지 여부
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static bool StateTreeNeedsRecompile(UStateTree* StateTree);

	//몽타주 슬롯 트랙의 슬롯 이름 변경 (SlotAnimTracks는 BlueprintReadOnly라 스크립트에서 직접 수정 불가)
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Animation")
	static bool SetMontageSlotName(UAnimMontage* Montage, int32 SlotTrackIndex, FName SlotName);

	//몽타주 섹션 조회 (CompositeSections는 스크립트에 노출되지 않음). 시작 시간순
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Animation")
	static void GetMontageSections(UAnimMontage* Montage, TArray<FName>& OutNames, TArray<float>& OutStartTimes, TArray<FName>& OutNextNames);

	//몽타주 섹션 전체 교체. 세 배열은 같은 인덱스끼리 한 섹션, 0초 섹션 필수, NextName이 None이면 연결 없음
	//실패 시 에러 메시지 반환, 성공 시 빈 문자열
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Animation")
	static FString SetMontageSections(UAnimMontage* Montage, const TArray<FName>& Names, const TArray<float>& StartTimes, const TArray<FName>& NextNames);

	//몽타주 슬롯 트랙의 세그먼트 전체 교체 (FAnimSegment 필드는 BlueprintReadOnly라 스크립트에서 직접 수정 불가)
	//네 배열은 같은 인덱스끼리 한 세그먼트, 몽타주 상 시작 위치(StartPos)는 앞 세그먼트 길이로 이어 붙여 자동 계산
	//몽타주 길이·섹션·노티파이 링크를 몽타주 에디터와 같은 방식으로 갱신. 실패 시 에러 메시지 반환, 성공 시 빈 문자열
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Animation")
	static FString SetMontageSegments(UAnimMontage* Montage, int32 SlotTrackIndex, const TArray<UAnimSequenceBase*>& Animations, const TArray<float>& AnimStartTimes, const TArray<float>& AnimEndTimes, const TArray<float>& PlayRates);

	//블렌드스페이스 샘플 검증 + 런타임 블렌드 데이터(세그먼트·삼각분할) 재계산
	//SampleData를 리플렉션으로 바꾸면 PostEditChangeProperty가 재계산하지 않아 추가·이동한 샘플이 블렌드에 반영되지 않음. 반환값은 샘플 수
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Animation")
	static int32 ResampleBlendSpace(UBlendSpace* BlendSpace);

	//스켈레톤에 이름 지정 가상 본 추가 (IK 본이 없는 스켈레톤에서 IK 목표 본으로 사용)
	//이름은 "VB " 접두사 필수. 같은 이름·소스·타깃이 이미 있으면 성공 처리. 실패 시 에러 메시지 반환, 성공 시 빈 문자열
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|Animation")
	static FString AddNamedVirtualBone(USkeleton* Skeleton, FName SourceBoneName, FName TargetBoneName, FName VirtualBoneName);

	//FontFace 에셋들을 기본 타입페이스로 묶은 런타임 폰트(UFont) 에셋 생성·갱신
	//FTypefaceEntry·FFontData 필드는 스크립트에 노출되지 않아 C++로 감쌈. 두 배열은 같은 인덱스끼리 한 타입페이스
	//같은 경로에 UFont가 있으면 타입페이스 목록을 교체. 저장은 하지 않음. 실패 시 에러 메시지 반환, 성공 시 빈 문자열
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|UI")
	static FString CreateFontFromFaces(const FString& FontAssetPath, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& FontFaces);

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
