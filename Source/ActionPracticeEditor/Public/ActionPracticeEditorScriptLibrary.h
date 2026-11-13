#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionPracticeEditorScriptLibrary.generated.h"

class UStateTree;
class UAnimMontage;

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
