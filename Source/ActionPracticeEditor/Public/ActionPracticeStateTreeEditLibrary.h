#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionPracticeStateTreeEditLibrary.generated.h"

class UStateTree;
class UStateTreeState;

//StateTree 에디터 데이터 구조 편집 함수 모음 (Python/MCP 툴세트용)
//스테이트 추가·삭제·이동·복사, 태스크/컨디션/트랜지션 추가·삭제·복사, 노드 프로퍼티 텍스트 설정
//가능한 경우 에디터와 공유되는 FStateTreeViewModel을 통해 편집하여 열린 StateTree 에디터와 Undo에 반영
//노드 프로퍼티 바인딩 설정·제거·조회. 편집 후 CompileStateTree로 런타임 데이터 갱신 필요
//bool + out 파라미터는 Python에서 false 시 None으로 축약되므로, 성공 여부만 있는 함수는 에러 문자열 반환(빈 문자열 = 성공)
UCLASS()
class ACTIONPRACTICEEDITOR_API UActionPracticeStateTreeEditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	//스테이트 계층, 태스크, 컨디션, 트랜지션을 노드 ID와 함께 들여쓰기 텍스트로 출력
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString DescribeStateTree(UStateTree* StateTree);

	//"Root/Combat/Attack" 형식의 이름 경로로 스테이트 검색
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static UStateTreeState* FindStateByPath(UStateTree* StateTree, const FString& StatePath);

	//스테이트의 이름 경로 ("Root/Combat/Attack")
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString GetStatePath(UStateTreeState* State);

	//스테이트 추가. ParentState가 null이면 최상위(서브트리)로 추가, Index가 음수면 맨 뒤
	//StateType: State, Group, Linked, LinkedAsset, Subtree
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static UStateTreeState* AddState(UStateTree* StateTree, UStateTreeState* ParentState, FName StateName, const FString& StateType, int32 Index, FString& OutError);

	//스테이트(하위 포함) 삭제. 남은 무효 바인딩 정리
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString RemoveState(UStateTree* StateTree, UStateTreeState* State);

	//스테이트 이동. Placement: Before, After, Into
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString MoveState(UStateTree* StateTree, UStateTreeState* State, UStateTreeState* TargetState, const FString& Placement);

	//다른(또는 같은) StateTree의 스테이트를 하위 포함 통째로 복사해 TargetParentState 자식 끝에 붙여넣기 (null이면 최상위)
	//노드 ID 재발급, 복사 범위 내부 트랜지션/바인딩은 새 ID로 재연결, 범위 밖을 가리키는 트랜지션은 수동 재지정 필요
	//시스템 클립보드를 경유하며 호출 전 내용을 복원
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static UStateTreeState* CopyStateFrom(UStateTree* SourceTree, UStateTreeState* SourceState, UStateTree* TargetTree, UStateTreeState* TargetParentState, FString& OutError);

	//태스크/컨디션 노드 추가 후 새 노드 ID 반환
	//NodeKind: Task, EnterCondition, TransitionCondition(TransitionID 필요)
	//NodeType: C++ 노드 구조체 경로(/Script/Module.Struct) 또는 BP 태스크/컨디션 클래스 경로(/Game/.../BP_X.BP_X_C)
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FGuid AddNode(UStateTree* StateTree, UStateTreeState* State, const FString& NodeKind, const FString& NodeType, FGuid TransitionID, FString& OutError);

	//노드 또는 트랜지션 삭제 (태스크, 진입/트랜지션 컨디션, 트랜지션 모두 ID로 지정)
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString RemoveNode(UStateTree* StateTree, UStateTreeState* State, FGuid NodeID);

	//다른(또는 같은) StateTree 스테이트의 노드/트랜지션을 TargetState로 복사. 새로 생긴 ID 목록 반환
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static TArray<FGuid> CopyNodeFrom(UStateTree* SourceTree, UStateTreeState* SourceState, FGuid SourceNodeID, UStateTree* TargetTree, UStateTreeState* TargetState, FString& OutError);

	//트랜지션 추가 후 ID 반환
	//Trigger: OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent, OnDelegate
	//TransitionType: GotoState, Succeeded, Failed, NextState, NextSelectableState, Parent ... (GotoState면 TargetState 필요)
	//EventTag: OnEvent일 때 필요한 게임플레이 태그 이름 (그 외 빈 문자열)
	//Priority: Low, Normal, Medium, High, Critical (빈 문자열이면 Normal)
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FGuid AddTransition(UStateTree* StateTree, UStateTreeState* State, const FString& Trigger, const FString& EventTag, const FString& TransitionType, UStateTreeState* TargetState, const FString& Priority, FString& OutError);

	//기존 트랜지션의 대상 변경 (GotoState면 TargetState 필요, 그 외 타입은 TargetState 무시)
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString SetTransitionTarget(UStateTree* StateTree, UStateTreeState* State, FGuid TransitionID, const FString& TransitionType, UStateTreeState* TargetState);

	//노드 프로퍼티를 언리얼 텍스트 포맷으로 설정 (예: "Radius", "100.0" / "AbilityClass", "/Game/.../BP_X.BP_X_C")
	//PropertyPath는 '.'로 중첩 구조체 지정. 인스턴스 데이터를 먼저 찾고 없으면 노드 구조체에서 찾음
	//트랜지션 ID를 주면 FStateTreeTransition 자체 필드(Priority, bDelayTransition, DelayDuration 등)를 설정
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString SetNodeProperty(UStateTree* StateTree, UStateTreeState* State, FGuid NodeID, const FString& PropertyPath, const FString& ValueText);

	//스테이트 자체 필드 설정 (Name, Tag, Type, SelectionBehavior, bEnabled 등). 언리얼 텍스트 포맷
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString SetStateProperty(UStateTree* StateTree, UStateTreeState* State, const FString& PropertyPath, const FString& ValueText);

	//노드 프로퍼티에 바인딩 설정 (같은 대상의 기존 바인딩은 교체)
	//State가 null이면 글로벌 노드(이밸류에이터·글로벌 태스크)를 대상으로 검색
	//SourceStruct: GetBindingSources가 출력하는 소스 이름(예: "Parameters"), "이름 [섹션]" 또는 소스 ID
	//같은 이름이 여럿이면 SourcePropertyPath가 해석되는 소스만 후보로 사용 (그래도 여럿이면 에러)
	//프로퍼티 경로는 '.' 구분 내부 이름. SourcePropertyPath가 비면 소스 구조체·컨텍스트 객체 통째 바인딩
	//대상에서 접근 가능한 소스인지, 타입 호환 여부를 검증
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString SetPropertyBinding(UStateTree* StateTree, UStateTreeState* State, FGuid TargetNodeID, const FString& TargetPropertyPath, const FString& SourceStruct, const FString& SourcePropertyPath);

	//노드 프로퍼티 바인딩 제거 (하위 경로 바인딩 포함)
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString RemovePropertyBinding(UStateTree* StateTree, UStateTreeState* State, FGuid TargetNodeID, const FString& TargetPropertyPath);

	//대상 노드에서 바인딩 가능한 소스 목록 (이름, 섹션, ID, 최상위 프로퍼티와 타입)과 대상 노드 자신의 프로퍼티
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString GetBindingSources(UStateTree* StateTree, UStateTreeState* State, FGuid TargetNodeID);

	//StateTree 전역 파라미터(루트 Parameters) 추가 또는 기본값 설정
	//없으면 Type(Bool, Byte, Int32, Int64, Float, Double, Name, String, Text)으로 추가, 이미 있으면 Type은 무시하고 값만 설정
	//ValueText는 언리얼 텍스트 포맷, 빈 문자열이면 값 유지. 숫자는 숫자 형식, Bool은 true/false/1/0만 허용
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString SetRootParameter(UStateTree* StateTree, FName ParameterName, const FString& Type, const FString& ValueText);

	//StateTree 전역 파라미터 삭제. 바인딩 소스로 사용 중이면 사용처를 담은 에러 반환
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString RemoveRootParameter(UStateTree* StateTree, FName ParameterName);

	//바인딩 목록 ("대상 노드.프로퍼티 <- 소스.프로퍼티"). State가 있으면 해당 스테이트 노드 대상만, null이면 전체
	UFUNCTION(BlueprintCallable, Category = "ActionPractice|Editor|StateTree")
	static FString DescribeBindings(UStateTree* StateTree, UStateTreeState* State);

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
