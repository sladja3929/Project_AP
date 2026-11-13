#include "ActionPracticeStateTreeEditLibrary.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/StringOutputDevice.h"
#include "GameplayTagContainer.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeViewModel.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "StateTreePropertyBindings.h"
#include "PropertyBindingPath.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingTypes.h"
#include "StructUtils/PropertyBag.h"
#include "StateTreeDelegates.h"
#include "Misc/DefaultValueHelper.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeStateTreeEditLibrary, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeStateTreeEditLibrary, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#define LOCTEXT_NAMESPACE "ActionPracticeStateTreeEditLibrary"

#pragma region "Internal Helpers"

namespace APStateTreeEdit
{
	UStateTreeEditorData* GetEditorData(UStateTree* StateTree)
	{
		return StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	}

	TSharedPtr<FStateTreeViewModel> GetViewModel(UStateTree* StateTree)
	{
		UStateTreeEditingSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>() : nullptr;

		if (!Subsystem || !StateTree)
		{
			return nullptr;
		}

		return Subsystem->FindOrAddViewModel(StateTree);
	}

	FString GuidToString(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::DigitsWithHyphens);
	}

	template<typename TEnum>
	bool ParseEnum(const FString& Text, TEnum& OutValue)
	{
		const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Text);

		if (Value == INDEX_NONE)
		{
			return false;
		}

		OutValue = static_cast<TEnum>(Value);
		return true;
	}

	template<typename TEnum>
	FString EnumToString(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	template<typename TEnum>
	FString EnumNameList()
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		TArray<FString> Names;

		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			Names.Add(Enum->GetNameStringByIndex(Index));
		}

		return FString::Join(Names, TEXT(", "));
	}

	//에디터 데이터 소속 검증. 실패 시 에러 메시지 반환
	FString ValidateState(UStateTree* StateTree, const UStateTreeState* State)
	{
		if (!GetEditorData(StateTree))
		{
			return TEXT("StateTree or its editor data is null");
		}

		if (!State)
		{
			return TEXT("State is null");
		}

		if (State->GetTypedOuter<UStateTree>() != StateTree)
		{
			return FString::Printf(TEXT("State %s does not belong to %s"), *State->Name.ToString(), *GetNameSafe(StateTree));
		}

		return FString();
	}

	TArray<TObjectPtr<UStateTreeState>>& GetSiblingArray(UStateTreeEditorData* EditorData, UStateTreeState* ParentState)
	{
		return ParentState ? ParentState->Children : EditorData->SubTrees;
	}

	//뷰모델 선택 상태를 작업 후 복원 (열린 에디터의 선택이 바뀌지 않도록)
	struct FScopedSelectionRestore
	{
		TSharedPtr<FStateTreeViewModel> ViewModel;
		TArray<TWeakObjectPtr<UStateTreeState>> SavedSelection;

		explicit FScopedSelectionRestore(TSharedPtr<FStateTreeViewModel> InViewModel)
			: ViewModel(InViewModel)
		{
			if (ViewModel.IsValid())
			{
				ViewModel->GetSelectedStates(SavedSelection);
			}
		}

		~FScopedSelectionRestore()
		{
			if (!ViewModel.IsValid())
			{
				return;
			}

			SavedSelection.RemoveAll([](const TWeakObjectPtr<UStateTreeState>& State) { return !State.IsValid(); });

			if (SavedSelection.IsEmpty())
			{
				ViewModel->ClearSelection();
			}
			else
			{
				ViewModel->SetSelection(SavedSelection);
			}
		}
	};

	//뷰모델 복사/붙여넣기가 시스템 클립보드를 사용하므로 사용자 클립보드 내용 복원
	struct FScopedClipboardRestore
	{
		FString SavedText;

		FScopedClipboardRestore()
		{
			FPlatformApplicationMisc::ClipboardPaste(SavedText);
		}

		~FScopedClipboardRestore()
		{
			FPlatformApplicationMisc::ClipboardCopy(*SavedText);
		}
	};

	//스테이트가 소유한 모든 에디터 노드 (태스크, 진입 컨디션, 고려사항, 트랜지션 컨디션)
	FStateTreeEditorNode* FindEditorNode(UStateTreeState* State, const FGuid& NodeID)
	{
		auto FindIn = [&NodeID](TArray<FStateTreeEditorNode>& Nodes) -> FStateTreeEditorNode*
		{
			return Nodes.FindByPredicate([&NodeID](const FStateTreeEditorNode& Node) { return Node.ID == NodeID; });
		};

		if (FStateTreeEditorNode* Found = FindIn(State->Tasks))
		{
			return Found;
		}

		if (FStateTreeEditorNode* Found = FindIn(State->EnterConditions))
		{
			return Found;
		}

		if (FStateTreeEditorNode* Found = FindIn(State->Considerations))
		{
			return Found;
		}

		if (State->SingleTask.ID == NodeID)
		{
			return &State->SingleTask;
		}

		for (FStateTreeTransition& Transition : State->Transitions)
		{
			if (FStateTreeEditorNode* Found = FindIn(Transition.Conditions))
			{
				return Found;
			}
		}

		return nullptr;
	}

	FStateTreeTransition* FindTransition(UStateTreeState* State, const FGuid& TransitionID)
	{
		return State->Transitions.FindByPredicate([&TransitionID](const FStateTreeTransition& Transition) { return Transition.ID == TransitionID; });
	}

	//복제한 노드에 새 ID 발급 및 UObject 인스턴스 데이터를 새 Outer로 복제 (원본과 공유 방지)
	FGuid MakeNodeUnique(FStateTreeEditorNode& Node, UObject* Outer)
	{
		Node.ID = FGuid::NewGuid();

		if (Node.InstanceObject)
		{
			Node.InstanceObject = DuplicateObject(Node.InstanceObject.Get(), Outer);
		}

		if (Node.ExecutionRuntimeDataObject)
		{
			Node.ExecutionRuntimeDataObject = DuplicateObject(Node.ExecutionRuntimeDataObject.Get(), Outer);
		}

		return Node.ID;
	}

	FProperty* FindPropertyByName(const UStruct* Struct, const FString& Name)
	{
		if (FProperty* Property = FindFProperty<FProperty>(Struct, FName(*Name)))
		{
			return Property;
		}

		//BP 변수 등 내부 이름과 표시 이름이 다른 경우 대비
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase) || It->GetAuthoredName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}

		return nullptr;
	}

	//'.' 구분 경로로 중첩 구조체를 따라가 마지막 프로퍼티에 텍스트 값 임포트
	FString ImportPropertyByPath(const UStruct* Struct, void* Memory, UObject* OwnerObject, const FString& PropertyPath, const FString& ValueText, FProperty** OutTopProperty = nullptr)
	{
		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."));

		if (Segments.IsEmpty())
		{
			return TEXT("PropertyPath is empty");
		}

		const UStruct* CurrentStruct = Struct;
		void* CurrentMemory = Memory;

		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			FProperty* Property = FindPropertyByName(CurrentStruct, Segments[Index]);

			if (!Property)
			{
				return FString::Printf(TEXT("Property '%s' not found on %s"), *Segments[Index], *CurrentStruct->GetName());
			}

			if (Index == 0 && OutTopProperty)
			{
				*OutTopProperty = Property;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentMemory);

			if (Index == Segments.Num() - 1)
			{
				FStringOutputDevice ErrorText;

				if (!Property->ImportText_Direct(*ValueText, ValuePtr, OwnerObject, PPF_None, &ErrorText))
				{
					return FString::Printf(TEXT("Failed to import '%s' into %s: %s"), *ValueText, *Property->GetName(), *ErrorText);
				}

				return FString();
			}

			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);

			if (!StructProperty)
			{
				return FString::Printf(TEXT("Property '%s' is not a struct; cannot descend"), *Segments[Index]);
			}

			CurrentStruct = StructProperty->Struct;
			CurrentMemory = ValuePtr;
		}

		return FString();
	}

	FString DescribeNode(const FStateTreeEditorNode& Node)
	{
		const UScriptStruct* NodeStruct = Node.Node.GetScriptStruct();
		const UStruct* InstanceStruct = Node.GetInstance().GetStruct();

		return FString::Printf(TEXT("%s <%s%s%s> id=%s"),
			*Node.GetName().ToString(),
			NodeStruct ? *NodeStruct->GetName() : TEXT("None"),
			InstanceStruct ? TEXT(" / ") : TEXT(""),
			InstanceStruct ? *InstanceStruct->GetName() : TEXT(""),
			*GuidToString(Node.ID));
	}

	void DescribeStateRecursive(const UStateTreeState* State, const FString& Path, int32 Depth, TArray<FString>& OutLines)
	{
		const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
		const FString StatePath = Path.IsEmpty() ? State->Name.ToString() : Path + TEXT("/") + State->Name.ToString();

		FString Header = FString::Printf(TEXT("%s* %s [%s, %s]"), *Indent, *State->Name.ToString(), *EnumToString(State->Type), *EnumToString(State->SelectionBehavior));

		if (State->Tag.IsValid())
		{
			Header += FString::Printf(TEXT(" tag=%s"), *State->Tag.ToString());
		}

		if (!State->bEnabled)
		{
			Header += TEXT(" (disabled)");
		}

		if (State->Type == EStateTreeStateType::Linked)
		{
			Header += FString::Printf(TEXT(" linked=%s"), *State->LinkedSubtree.Name.ToString());
		}
		else if (State->Type == EStateTreeStateType::LinkedAsset)
		{
			Header += FString::Printf(TEXT(" linkedAsset=%s"), *GetPathNameSafe(State->LinkedAsset));
		}

		Header += FString::Printf(TEXT(" path=%s"), *StatePath);
		OutLines.Add(Header);

		for (const FStateTreeEditorNode& Node : State->EnterConditions)
		{
			OutLines.Add(FString::Printf(TEXT("%s    EnterCondition: %s"), *Indent, *DescribeNode(Node)));
		}

		for (const FStateTreeEditorNode& Node : State->Tasks)
		{
			OutLines.Add(FString::Printf(TEXT("%s    Task: %s"), *Indent, *DescribeNode(Node)));
		}

		for (const FStateTreeTransition& Transition : State->Transitions)
		{
			FString Line = FString::Printf(TEXT("%s    Transition: %s"), *Indent, *EnumToString(Transition.Trigger));

			if (Transition.RequiredEvent.Tag.IsValid())
			{
				Line += FString::Printf(TEXT(" [%s]"), *Transition.RequiredEvent.Tag.ToString());
			}

			Line += FString::Printf(TEXT(" -> %s"), *EnumToString(Transition.State.LinkType));

			if (Transition.State.LinkType == EStateTreeTransitionType::GotoState)
			{
				Line += FString::Printf(TEXT(" %s"), *Transition.State.Name.ToString());
			}

			Line += FString::Printf(TEXT(" priority=%s"), *EnumToString(Transition.Priority));

			if (Transition.bDelayTransition)
			{
				Line += FString::Printf(TEXT(" delay=%.2f"), Transition.DelayDuration);
			}

			if (!Transition.bTransitionEnabled)
			{
				Line += TEXT(" (disabled)");
			}

			Line += FString::Printf(TEXT(" id=%s"), *GuidToString(Transition.ID));
			OutLines.Add(Line);

			for (const FStateTreeEditorNode& Node : Transition.Conditions)
			{
				OutLines.Add(FString::Printf(TEXT("%s        Condition: %s"), *Indent, *DescribeNode(Node)));
			}
		}

		for (const UStateTreeState* Child : State->Children)
		{
			if (Child)
			{
				DescribeStateRecursive(Child, StatePath, Depth + 1, OutLines);
			}
		}
	}

	//바인딩 대상 노드 검색. State가 null이면 글로벌 노드(이밸류에이터, 글로벌 태스크)
	FStateTreeEditorNode* FindBindingTargetNode(UStateTreeEditorData* EditorData, UStateTreeState* State, const FGuid& NodeID)
	{
		if (State)
		{
			return FindEditorNode(State, NodeID);
		}

		auto FindIn = [&NodeID](TArray<FStateTreeEditorNode>& Nodes) -> FStateTreeEditorNode*
		{
			return Nodes.FindByPredicate([&NodeID](const FStateTreeEditorNode& Node) { return Node.ID == NodeID; });
		};

		if (FStateTreeEditorNode* Found = FindIn(EditorData->Evaluators))
		{
			return Found;
		}

		return FindIn(EditorData->GlobalTasks);
	}

	//바인딩 대상 검증 공통 처리. 성공 시 OutNode 설정
	FString ValidateBindingTarget(UStateTree* StateTree, UStateTreeState* State, const FGuid& NodeID, FStateTreeEditorNode*& OutNode)
	{
		UStateTreeEditorData* EditorData = GetEditorData(StateTree);

		if (!EditorData)
		{
			return TEXT("StateTree or its editor data is null");
		}

		if (State)
		{
			const FString Error = ValidateState(StateTree, State);

			if (!Error.IsEmpty())
			{
				return Error;
			}
		}

		OutNode = FindBindingTargetNode(EditorData, State, NodeID);

		if (!OutNode)
		{
			return FString::Printf(TEXT("Node %s not found on %s"), *GuidToString(NodeID), State ? *State->Name.ToString() : TEXT("global nodes"));
		}

		return FString();
	}

	//구조체 최상위 프로퍼티 "이름(타입)" 목록
	//클래스(컨텍스트 액터·컴포넌트)는 엔진 상속 멤버가 수백 개라 해당 클래스 계층의 에디터 노출 프로퍼티만, 최대 개수 제한
	FString DescribeStructProperties(const UStruct* Struct)
	{
		if (!Struct)
		{
			return TEXT("(no data)");
		}

		constexpr int32 MaxEntries = 40;
		const bool bIsClass = Struct->IsA<UClass>();
		TArray<FString> Entries;
		int32 NumSkipped = 0;

		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (bIsClass && !It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				continue;
			}

			if (Entries.Num() >= MaxEntries)
			{
				++NumSkipped;
				continue;
			}

			Entries.Add(FString::Printf(TEXT("%s(%s)"), *It->GetName(), *It->GetCPPType()));
		}

		FString Result = Entries.IsEmpty() ? TEXT("(no properties)") : FString::Join(Entries, TEXT(", "));

		if (NumSkipped > 0)
		{
			Result += FString::Printf(TEXT(", ... (+%d more)"), NumSkipped);
		}

		return Result;
	}

	FString DescribeBindableStruct(const FPropertyBindingBindableStructDescriptor& Desc)
	{
		const FString Section = Desc.GetSection();
		const FString SectionText = Section.IsEmpty() ? FString() : FString::Printf(TEXT(" [%s]"), *Section);

		return FString::Printf(TEXT("%s%s id=%s"), *Desc.Name.ToString(), *SectionText, *GuidToString(Desc.ID));
	}

	//구조체 ID의 표시 이름 (찾지 못하면 ID)
	FString GetBindableStructName(const UStateTreeEditorData* EditorData, const FGuid& StructID)
	{
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Desc;

		if (EditorData->GetBindableStructByID(StructID, Desc) && Desc.IsValid())
		{
			return Desc.Get<FPropertyBindingBindableStructDescriptor>().Name.ToString();
		}

		return GuidToString(StructID);
	}

	//경로를 구조체 실제 데이터 기준으로 해석해 세그먼트 갱신 후 마지막 프로퍼티 반환. 실패 시 에러 메시지
	//bAllowEmpty면 빈 경로(구조체·컨텍스트 객체 통째 바인딩) 허용, 이때 OutLeafProperty는 null
	//컨텍스트 객체처럼 에디터 시점 데이터가 없는 소스는 FallbackStruct(디스크립터 타입)로 정적 해석
	FString ResolveBindingPath(const UStateTreeEditorData* EditorData, FPropertyBindingPath& Path, const FString& PathText, const FProperty*& OutLeafProperty, bool bAllowEmpty = false, const UStruct* FallbackStruct = nullptr)
	{
		OutLeafProperty = nullptr;
		FPropertyBindingDataView DataView;
		const bool bHasData = EditorData->GetBindingDataViewByID(Path.GetStructID(), DataView) && DataView.IsValid();
		const UStruct* BaseStruct = bHasData ? DataView.GetStruct() : FallbackStruct;

		if (!BaseStruct)
		{
			return FString::Printf(TEXT("No data found for struct %s"), *GuidToString(Path.GetStructID()));
		}

		if (PathText.IsEmpty())
		{
			return bAllowEmpty ? FString() : TEXT("Property path is empty");
		}

		if (!Path.FromString(PathText))
		{
			return FString::Printf(TEXT("Invalid property path '%s'"), *PathText);
		}

		TArray<FPropertyBindingPathIndirection> Indirections;
		FString ResolveError;

		const bool bResolved = bHasData
			? Path.ResolveIndirectionsWithValue(DataView, Indirections, &ResolveError)
			: Path.ResolveIndirections(BaseStruct, Indirections, &ResolveError);

		if (!bResolved || Indirections.IsEmpty())
		{
			return FString::Printf(TEXT("Cannot resolve '%s': %s (properties: %s)"), *PathText, *ResolveError, *DescribeStructProperties(BaseStruct));
		}

		Path.UpdateSegmentsFromIndirections(Indirections);
		OutLeafProperty = Indirections.Last().GetProperty();
		return FString();
	}

	//빈 소스 경로(구조체·컨텍스트 객체 통째) 바인딩의 타입 호환 검사
	bool IsWholeStructCompatible(const UStruct* SourceStruct, const FProperty* TargetProperty)
	{
		if (!SourceStruct)
		{
			return false;
		}

		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(TargetProperty))
		{
			const UClass* SourceClass = Cast<UClass>(SourceStruct);
			return SourceClass && ObjectProperty->PropertyClass && SourceClass->IsChildOf(ObjectProperty->PropertyClass);
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(TargetProperty))
		{
			return SourceStruct->IsChildOf(StructProperty->Struct);
		}

		return false;
	}

	//소스 지정 문자열 매칭: 이름, "이름 [섹션]", ID
	bool MatchesSourceStruct(const FPropertyBindingBindableStructDescriptor& Desc, const FString& SourceStruct)
	{
		const FString Name = Desc.Name.ToString();
		const FString NameWithSection = FString::Printf(TEXT("%s [%s]"), *Name, *Desc.GetSection());

		return Name.Equals(SourceStruct, ESearchCase::IgnoreCase)
			|| NameWithSection.Equals(SourceStruct, ESearchCase::IgnoreCase)
			|| GuidToString(Desc.ID).Equals(SourceStruct, ESearchCase::IgnoreCase);
	}
}

#pragma endregion

#pragma region "Query"

FString UActionPracticeStateTreeEditLibrary::DescribeStateTree(UStateTree* StateTree)
{
	const UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	if (!EditorData)
	{
		return TEXT("StateTree or its editor data is null");
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("StateTree %s schema=%s"), *GetPathNameSafe(StateTree), EditorData->Schema ? *EditorData->Schema->GetClass()->GetName() : TEXT("None")));

	for (const FStateTreeEditorNode& Node : EditorData->Evaluators)
	{
		Lines.Add(FString::Printf(TEXT("  Evaluator: %s"), *APStateTreeEdit::DescribeNode(Node)));
	}

	for (const FStateTreeEditorNode& Node : EditorData->GlobalTasks)
	{
		Lines.Add(FString::Printf(TEXT("  GlobalTask: %s"), *APStateTreeEdit::DescribeNode(Node)));
	}

	for (const UStateTreeState* SubTree : EditorData->SubTrees)
	{
		if (SubTree)
		{
			APStateTreeEdit::DescribeStateRecursive(SubTree, FString(), 0, Lines);
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

UStateTreeState* UActionPracticeStateTreeEditLibrary::FindStateByPath(UStateTree* StateTree, const FString& StatePath)
{
	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	if (!EditorData)
	{
		return nullptr;
	}

	TArray<FString> Segments;
	StatePath.ParseIntoArray(Segments, TEXT("/"));

	TArray<TObjectPtr<UStateTreeState>>* Candidates = &EditorData->SubTrees;
	UStateTreeState* Found = nullptr;

	for (const FString& Segment : Segments)
	{
		Found = nullptr;

		for (UStateTreeState* Candidate : *Candidates)
		{
			if (Candidate && Candidate->Name.ToString().Equals(Segment, ESearchCase::IgnoreCase))
			{
				Found = Candidate;
				break;
			}
		}

		if (!Found)
		{
			return nullptr;
		}

		Candidates = &Found->Children;
	}

	return Found;
}

FString UActionPracticeStateTreeEditLibrary::GetStatePath(UStateTreeState* State)
{
	TArray<FString> Names;

	for (const UStateTreeState* Current = State; Current; Current = Current->Parent)
	{
		Names.Insert(Current->Name.ToString(), 0);
	}

	return FString::Join(Names, TEXT("/"));
}

#pragma endregion

#pragma region "States"

UStateTreeState* UActionPracticeStateTreeEditLibrary::AddState(UStateTree* StateTree, UStateTreeState* ParentState, FName StateName, const FString& StateType, int32 Index, FString& OutError)
{
	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	if (!EditorData)
	{
		OutError = TEXT("StateTree or its editor data is null");
		return nullptr;
	}

	if (ParentState)
	{
		OutError = APStateTreeEdit::ValidateState(StateTree, ParentState);

		if (!OutError.IsEmpty())
		{
			return nullptr;
		}
	}

	EStateTreeStateType Type = EStateTreeStateType::State;

	if (!StateType.IsEmpty() && !APStateTreeEdit::ParseEnum(StateType, Type))
	{
		OutError = FString::Printf(TEXT("Unknown StateType '%s' (valid: %s)"), *StateType, *APStateTreeEdit::EnumNameList<EStateTreeStateType>());
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTransaction", "Add State"));

	UObject* Outer = ParentState ? static_cast<UObject*>(ParentState) : static_cast<UObject*>(EditorData);
	Outer->Modify();

	UStateTreeState* NewState = NewObject<UStateTreeState>(Outer, FName(), RF_Transactional);
	NewState->Name = StateName;
	NewState->Type = Type;
	NewState->Parent = ParentState;

	TArray<TObjectPtr<UStateTreeState>>& Siblings = APStateTreeEdit::GetSiblingArray(EditorData, ParentState);
	Siblings.Insert(NewState, (Index < 0 || Index > Siblings.Num()) ? Siblings.Num() : Index);

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
	{
		ViewModel->GetOnStateAdded().Broadcast(ParentState, NewState);
	}

	DEBUG_LOG(TEXT("AddState: %s under %s"), *StateName.ToString(), ParentState ? *ParentState->Name.ToString() : TEXT("(root)"));
	return NewState;
}

FString UActionPracticeStateTreeEditLibrary::RemoveState(UStateTree* StateTree, UStateTreeState* State)
{
	FString Error = APStateTreeEdit::ValidateState(StateTree, State);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree);

	if (!ViewModel.IsValid())
	{
		return TEXT("StateTree view model unavailable");
	}

	const FName StateName = State->Name;

	{
		APStateTreeEdit::FScopedSelectionRestore SelectionRestore(ViewModel);
		ViewModel->SetSelection(State);
		ViewModel->RemoveSelectedStates();
	}

	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);
	EditorData->RemoveInvalidBindings();

	DEBUG_LOG(TEXT("RemoveState: %s"), *StateName.ToString());
	return FString();
}

FString UActionPracticeStateTreeEditLibrary::MoveState(UStateTree* StateTree, UStateTreeState* State, UStateTreeState* TargetState, const FString& Placement)
{
	FString Error = APStateTreeEdit::ValidateState(StateTree, State);

	if (Error.IsEmpty())
	{
		Error = APStateTreeEdit::ValidateState(StateTree, TargetState);
	}

	if (!Error.IsEmpty())
	{
		return Error;
	}

	if (State == TargetState)
	{
		return TEXT("State and TargetState are the same");
	}

	//자기 하위로 이동 금지
	for (const UStateTreeState* Ancestor = TargetState; Ancestor; Ancestor = Ancestor->Parent)
	{
		if (Ancestor == State)
		{
			return TEXT("TargetState is inside State");
		}
	}

	TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree);

	if (!ViewModel.IsValid())
	{
		return TEXT("StateTree view model unavailable");
	}

	APStateTreeEdit::FScopedSelectionRestore SelectionRestore(ViewModel);
	ViewModel->SetSelection(State);

	if (Placement.Equals(TEXT("Before"), ESearchCase::IgnoreCase))
	{
		ViewModel->MoveSelectedStatesBefore(TargetState);
	}
	else if (Placement.Equals(TEXT("After"), ESearchCase::IgnoreCase))
	{
		ViewModel->MoveSelectedStatesAfter(TargetState);
	}
	else if (Placement.Equals(TEXT("Into"), ESearchCase::IgnoreCase))
	{
		ViewModel->MoveSelectedStatesInto(TargetState);
	}
	else
	{
		return FString::Printf(TEXT("Unknown Placement '%s' (valid: Before, After, Into)"), *Placement);
	}

	DEBUG_LOG(TEXT("MoveState: %s %s %s"), *State->Name.ToString(), *Placement, *TargetState->Name.ToString());
	return FString();
}

UStateTreeState* UActionPracticeStateTreeEditLibrary::CopyStateFrom(UStateTree* SourceTree, UStateTreeState* SourceState, UStateTree* TargetTree, UStateTreeState* TargetParentState, FString& OutError)
{
	OutError = APStateTreeEdit::ValidateState(SourceTree, SourceState);

	if (OutError.IsEmpty() && TargetParentState)
	{
		OutError = APStateTreeEdit::ValidateState(TargetTree, TargetParentState);
	}

	UStateTreeEditorData* TargetEditorData = APStateTreeEdit::GetEditorData(TargetTree);

	if (OutError.IsEmpty() && !TargetEditorData)
	{
		OutError = TEXT("TargetTree or its editor data is null");
	}

	if (!OutError.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FStateTreeViewModel> SourceViewModel = APStateTreeEdit::GetViewModel(SourceTree);
	TSharedPtr<FStateTreeViewModel> TargetViewModel = APStateTreeEdit::GetViewModel(TargetTree);

	if (!SourceViewModel.IsValid() || !TargetViewModel.IsValid())
	{
		OutError = TEXT("StateTree view model unavailable");
		return nullptr;
	}

	const TArray<TObjectPtr<UStateTreeState>> Before = APStateTreeEdit::GetSiblingArray(TargetEditorData, TargetParentState);

	{
		APStateTreeEdit::FScopedClipboardRestore ClipboardRestore;
		APStateTreeEdit::FScopedSelectionRestore SelectionRestore(SourceViewModel);

		SourceViewModel->SetSelection(SourceState);
		SourceViewModel->CopySelectedStates();
		TargetViewModel->PasteStatesAsChildrenFromClipboard(TargetParentState);
	}

	for (UStateTreeState* Candidate : APStateTreeEdit::GetSiblingArray(TargetEditorData, TargetParentState))
	{
		if (Candidate && !Before.Contains(Candidate))
		{
			DEBUG_LOG(TEXT("CopyStateFrom: %s.%s -> %s"), *GetNameSafe(SourceTree), *SourceState->Name.ToString(), *GetNameSafe(TargetTree));
			return Candidate;
		}
	}

	OutError = TEXT("Paste produced no new state");
	return nullptr;
}

FString UActionPracticeStateTreeEditLibrary::SetStateProperty(UStateTree* StateTree, UStateTreeState* State, const FString& PropertyPath, const FString& ValueText)
{
	FString Error = APStateTreeEdit::ValidateState(StateTree, State);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetStatePropertyTransaction", "Set State Property"));
	State->Modify();

	FProperty* TopProperty = nullptr;
	Error = APStateTreeEdit::ImportPropertyByPath(UStateTreeState::StaticClass(), State, State, PropertyPath, ValueText, &TopProperty);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	//Type 변경 등 스테이트 측 후처리(링크/파라미터 정리) 실행
	FPropertyChangedEvent ChangedEvent(TopProperty, EPropertyChangeType::ValueSet);
	State->PostEditChangeProperty(ChangedEvent);

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
	{
		ViewModel->NotifyStatesChangedExternally({ State }, ChangedEvent);
	}

	return FString();
}

#pragma endregion

#pragma region "Nodes"

FGuid UActionPracticeStateTreeEditLibrary::AddNode(UStateTree* StateTree, UStateTreeState* State, const FString& NodeKind, const FString& NodeType, FGuid TransitionID, FString& OutError)
{
	OutError = APStateTreeEdit::ValidateState(StateTree, State);

	if (!OutError.IsEmpty())
	{
		return FGuid();
	}

	const bool bIsTask = NodeKind.Equals(TEXT("Task"), ESearchCase::IgnoreCase);
	const bool bIsEnterCondition = NodeKind.Equals(TEXT("EnterCondition"), ESearchCase::IgnoreCase);
	const bool bIsTransitionCondition = NodeKind.Equals(TEXT("TransitionCondition"), ESearchCase::IgnoreCase);

	if (!bIsTask && !bIsEnterCondition && !bIsTransitionCondition)
	{
		OutError = FString::Printf(TEXT("Unknown NodeKind '%s' (valid: Task, EnterCondition, TransitionCondition)"), *NodeKind);
		return FGuid();
	}

	FStateTreeTransition* Transition = nullptr;

	if (bIsTransitionCondition)
	{
		Transition = APStateTreeEdit::FindTransition(State, TransitionID);

		if (!Transition)
		{
			OutError = FString::Printf(TEXT("Transition %s not found on state %s"), *APStateTreeEdit::GuidToString(TransitionID), *State->Name.ToString());
			return FGuid();
		}
	}

	//C++ 노드 구조체 또는 BP 노드 클래스
	UObject* TypeObject = StaticLoadObject(UObject::StaticClass(), nullptr, *NodeType);
	const UScriptStruct* NodeStruct = Cast<UScriptStruct>(TypeObject);
	UClass* NodeClass = Cast<UClass>(TypeObject);

	const UScriptStruct* RequiredStruct = bIsTask ? FStateTreeTaskBase::StaticStruct() : FStateTreeConditionBase::StaticStruct();
	const UClass* RequiredClass = bIsTask ? UStateTreeTaskBlueprintBase::StaticClass() : UStateTreeConditionBlueprintBase::StaticClass();

	if (!(NodeStruct && NodeStruct->IsChildOf(RequiredStruct)) && !(NodeClass && NodeClass->IsChildOf(RequiredClass)))
	{
		OutError = FString::Printf(TEXT("NodeType '%s' is not a %s struct or %s class"), *NodeType, *RequiredStruct->GetName(), *RequiredClass->GetName());
		return FGuid();
	}

	const FScopedTransaction ScopedTransaction(LOCTEXT("AddNodeTransaction", "Add StateTree Node"));
	State->Modify();

	TArray<FStateTreeEditorNode>& Nodes = bIsTask ? State->Tasks : (bIsEnterCondition ? State->EnterConditions : Transition->Conditions);
	FStateTreeEditorNode& NewNode = Nodes.AddDefaulted_GetRef();

	if (NodeStruct)
	{
		NewNode.InitializeAs(State, NodeStruct);
	}
	else if (bIsTask)
	{
		FStateTreeBlueprintTaskWrapper TaskWrapper;
		TaskWrapper.TaskClass = NodeClass;
		NewNode.InitializeAs<FStateTreeBlueprintTaskWrapper>(State, MoveTemp(TaskWrapper));
	}
	else
	{
		FStateTreeBlueprintConditionWrapper ConditionWrapper;
		ConditionWrapper.ConditionClass = NodeClass;
		NewNode.InitializeAs<FStateTreeBlueprintConditionWrapper>(State, MoveTemp(ConditionWrapper));
	}

	const FGuid NewID = NewNode.ID;
	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
	{
		ViewModel->GetOnStateNodesChanged().Broadcast(State);
	}

	DEBUG_LOG(TEXT("AddNode: %s %s on %s"), *NodeKind, *NodeType, *State->Name.ToString());
	return NewID;
}

FString UActionPracticeStateTreeEditLibrary::RemoveNode(UStateTree* StateTree, UStateTreeState* State, FGuid NodeID)
{
	FString Error = APStateTreeEdit::ValidateState(StateTree, State);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	if (!APStateTreeEdit::FindEditorNode(State, NodeID) && !APStateTreeEdit::FindTransition(State, NodeID))
	{
		return FString::Printf(TEXT("Node or transition %s not found on state %s"), *APStateTreeEdit::GuidToString(NodeID), *State->Name.ToString());
	}

	TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree);

	if (!ViewModel.IsValid())
	{
		return TEXT("StateTree view model unavailable");
	}

	//트랜지션 컨디션은 뷰모델 DeleteNode가 찾지 못하므로 직접 제거
	FStateTreeTransition* OwnerTransition = State->Transitions.FindByPredicate([&NodeID](const FStateTreeTransition& Transition)
	{
		return Transition.Conditions.ContainsByPredicate([&NodeID](const FStateTreeEditorNode& Node) { return Node.ID == NodeID; });
	});

	if (OwnerTransition)
	{
		const FScopedTransaction ScopedTransaction(LOCTEXT("RemoveTransitionConditionTransaction", "Remove Transition Condition"));
		State->Modify();

		OwnerTransition->Conditions.RemoveAll([&NodeID](const FStateTreeEditorNode& Node) { return Node.ID == NodeID; });
		APStateTreeEdit::GetEditorData(StateTree)->RemoveInvalidBindings();

		UStateTreeEditingSubsystem::MarkAsModified(StateTree);
		ViewModel->GetOnStateNodesChanged().Broadcast(State);
	}
	else
	{
		ViewModel->DeleteNode(State, NodeID);
	}

	if (APStateTreeEdit::FindEditorNode(State, NodeID) || APStateTreeEdit::FindTransition(State, NodeID))
	{
		return TEXT("DeleteNode did not remove the node");
	}

	return FString();
}

TArray<FGuid> UActionPracticeStateTreeEditLibrary::CopyNodeFrom(UStateTree* SourceTree, UStateTreeState* SourceState, FGuid SourceNodeID, UStateTree* TargetTree, UStateTreeState* TargetState, FString& OutError)
{
	TArray<FGuid> NewIDs;
	OutError = APStateTreeEdit::ValidateState(SourceTree, SourceState);

	if (OutError.IsEmpty())
	{
		OutError = APStateTreeEdit::ValidateState(TargetTree, TargetState);
	}

	if (!OutError.IsEmpty())
	{
		return NewIDs;
	}

	const FStateTreeEditorNode* SourceNode = APStateTreeEdit::FindEditorNode(SourceState, SourceNodeID);
	const FStateTreeTransition* SourceTransition = SourceNode ? nullptr : APStateTreeEdit::FindTransition(SourceState, SourceNodeID);

	if (!SourceNode && !SourceTransition)
	{
		OutError = FString::Printf(TEXT("Node or transition %s not found on state %s"), *APStateTreeEdit::GuidToString(SourceNodeID), *SourceState->Name.ToString());
		return NewIDs;
	}

	//뷰모델 CopyNode/PasteNodes는 시스템 클립보드 접근이 실패할 수 있어 구조체를 직접 복제 (바인딩은 복사하지 않음)
	const FScopedTransaction ScopedTransaction(LOCTEXT("CopyNodeTransaction", "Copy StateTree Node"));
	TargetState->Modify();

	if (SourceNode)
	{
		//원본과 같은 종류의 컨테이너로만 복사 (태스크 → Tasks, 진입 컨디션 → EnterConditions)
		TArray<FStateTreeEditorNode>* TargetArray = nullptr;

		if (SourceState->Tasks.ContainsByPredicate([&SourceNodeID](const FStateTreeEditorNode& Node) { return Node.ID == SourceNodeID; }))
		{
			TargetArray = &TargetState->Tasks;
		}
		else if (SourceState->EnterConditions.ContainsByPredicate([&SourceNodeID](const FStateTreeEditorNode& Node) { return Node.ID == SourceNodeID; }))
		{
			TargetArray = &TargetState->EnterConditions;
		}

		if (!TargetArray)
		{
			OutError = TEXT("Only tasks and enter conditions can be copied individually; copy the whole transition to copy its conditions");
			return NewIDs;
		}

		//같은 스테이트로 복사 시 Add 재할당으로 원본 참조가 무효화되므로 값 복사 후 추가
		FStateTreeEditorNode NodeCopy = *SourceNode;
		FStateTreeEditorNode& NewNode = TargetArray->Add_GetRef(MoveTemp(NodeCopy));
		NewIDs.Add(APStateTreeEdit::MakeNodeUnique(NewNode, TargetState));
	}
	else
	{
		FStateTreeTransition TransitionCopy = *SourceTransition;
		FStateTreeTransition& NewTransition = TargetState->Transitions.Add_GetRef(MoveTemp(TransitionCopy));
		NewTransition.ID = FGuid::NewGuid();
		NewIDs.Add(NewTransition.ID);

		for (FStateTreeEditorNode& Condition : NewTransition.Conditions)
		{
			NewIDs.Add(APStateTreeEdit::MakeNodeUnique(Condition, TargetState));
		}

		if (SourceTree != TargetTree && NewTransition.State.LinkType == EStateTreeTransitionType::GotoState)
		{
			DEBUG_LOG(TEXT("CopyNodeFrom: transition target '%s' still references the source tree; re-target it"), *NewTransition.State.Name.ToString());
		}
	}

	UStateTreeEditingSubsystem::MarkAsModified(TargetTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(TargetTree))
	{
		ViewModel->GetOnStateNodesChanged().Broadcast(TargetState);
	}

	DEBUG_LOG(TEXT("CopyNodeFrom: %s.%s -> %s.%s (%d ids)"), *GetNameSafe(SourceTree), *SourceState->Name.ToString(), *GetNameSafe(TargetTree), *TargetState->Name.ToString(), NewIDs.Num());
	return NewIDs;
}

FString UActionPracticeStateTreeEditLibrary::SetNodeProperty(UStateTree* StateTree, UStateTreeState* State, FGuid NodeID, const FString& PropertyPath, const FString& ValueText)
{
	FString Error = APStateTreeEdit::ValidateState(StateTree, State);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	FStateTreeEditorNode* Node = APStateTreeEdit::FindEditorNode(State, NodeID);
	FStateTreeTransition* Transition = Node ? nullptr : APStateTreeEdit::FindTransition(State, NodeID);

	if (!Node && !Transition)
	{
		return FString::Printf(TEXT("Node or transition %s not found on state %s"), *APStateTreeEdit::GuidToString(NodeID), *State->Name.ToString());
	}

	const FScopedTransaction ScopedTransaction(LOCTEXT("SetNodePropertyTransaction", "Set StateTree Node Property"));
	State->Modify();

	if (Transition)
	{
		Error = APStateTreeEdit::ImportPropertyByPath(FStateTreeTransition::StaticStruct(), Transition, State, PropertyPath, ValueText);
	}
	else
	{
		//인스턴스 데이터(태스크 파라미터) 우선, 없으면 노드 구조체
		FStateTreeDataView InstanceView = Node->GetInstance();

		if (Node->InstanceObject)
		{
			Node->InstanceObject->Modify();
		}

		//첫 경로 세그먼트가 인스턴스 데이터에 있으면 인스턴스만 대상 (값 오류 시 노드 구조체로 폴백하지 않음)
		FString FirstSegment;
		PropertyPath.Split(TEXT("."), &FirstSegment, nullptr);
		FirstSegment = FirstSegment.IsEmpty() ? PropertyPath : FirstSegment;

		const bool bInInstance = InstanceView.IsValid() && APStateTreeEdit::FindPropertyByName(InstanceView.GetStruct(), FirstSegment);

		//에디터 노드 필드(ExpressionOperand, ExpressionIndent)는 컴파일러가 에디터 노드에서 읽으므로 노드 구조체보다 우선
		//Node/Instance 등 다른 에디터 노드 필드는 노출하지 않도록 두 필드만 허용
		const bool bInEditorNode = !bInInstance
			&& (FirstSegment == GET_MEMBER_NAME_STRING_CHECKED(FStateTreeEditorNode, ExpressionOperand) || FirstSegment == GET_MEMBER_NAME_STRING_CHECKED(FStateTreeEditorNode, ExpressionIndent));

		if (bInEditorNode)
		{
			Error = APStateTreeEdit::ImportPropertyByPath(FStateTreeEditorNode::StaticStruct(), Node, State, PropertyPath, ValueText);
		}
		else if (bInInstance)
		{
			Error = APStateTreeEdit::ImportPropertyByPath(InstanceView.GetStruct(), InstanceView.GetMutableMemory(), Node->InstanceObject ? Node->InstanceObject.Get() : State, PropertyPath, ValueText);

			if (Error.IsEmpty() && Node->InstanceObject)
			{
				//BP 태스크/컨디션 인스턴스의 에디터 후처리
				Node->InstanceObject->PostEditChange();
			}
		}
		else if (Node->Node.IsValid())
		{
			Error = APStateTreeEdit::ImportPropertyByPath(Node->Node.GetScriptStruct(), Node->Node.GetMutableMemory(), State, PropertyPath, ValueText);
		}
		else
		{
			Error = TEXT("Node has no data");
		}
	}

	if (!Error.IsEmpty())
	{
		return Error;
	}

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
	{
		ViewModel->GetOnStateNodesChanged().Broadcast(State);
	}

	return FString();
}

#pragma endregion

#pragma region "Transitions"

FGuid UActionPracticeStateTreeEditLibrary::AddTransition(UStateTree* StateTree, UStateTreeState* State, const FString& Trigger, const FString& EventTag, const FString& TransitionType, UStateTreeState* TargetState, const FString& Priority, FString& OutError)
{
	OutError = APStateTreeEdit::ValidateState(StateTree, State);

	if (OutError.IsEmpty() && TargetState)
	{
		OutError = APStateTreeEdit::ValidateState(StateTree, TargetState);
	}

	if (!OutError.IsEmpty())
	{
		return FGuid();
	}

	EStateTreeTransitionTrigger TriggerValue = EStateTreeTransitionTrigger::OnStateCompleted;
	EStateTreeTransitionType TypeValue = EStateTreeTransitionType::GotoState;
	EStateTreeTransitionPriority PriorityValue = EStateTreeTransitionPriority::Normal;

	if (!APStateTreeEdit::ParseEnum(Trigger, TriggerValue))
	{
		OutError = FString::Printf(TEXT("Unknown Trigger '%s' (valid: %s)"), *Trigger, *APStateTreeEdit::EnumNameList<EStateTreeTransitionTrigger>());
		return FGuid();
	}

	if (!APStateTreeEdit::ParseEnum(TransitionType, TypeValue))
	{
		OutError = FString::Printf(TEXT("Unknown TransitionType '%s' (valid: %s)"), *TransitionType, *APStateTreeEdit::EnumNameList<EStateTreeTransitionType>());
		return FGuid();
	}

	if (!Priority.IsEmpty() && !APStateTreeEdit::ParseEnum(Priority, PriorityValue))
	{
		OutError = FString::Printf(TEXT("Unknown Priority '%s' (valid: %s)"), *Priority, *APStateTreeEdit::EnumNameList<EStateTreeTransitionPriority>());
		return FGuid();
	}

	if (TypeValue == EStateTreeTransitionType::GotoState && !TargetState)
	{
		OutError = TEXT("GotoState transition requires TargetState");
		return FGuid();
	}

	FGameplayTag EventTagValue;

	if (!EventTag.IsEmpty())
	{
		EventTagValue = FGameplayTag::RequestGameplayTag(FName(*EventTag), false);

		if (!EventTagValue.IsValid())
		{
			OutError = FString::Printf(TEXT("Gameplay tag '%s' is not registered"), *EventTag);
			return FGuid();
		}
	}

	if (TriggerValue == EStateTreeTransitionTrigger::OnEvent && !EventTagValue.IsValid())
	{
		OutError = TEXT("OnEvent transition requires EventTag");
		return FGuid();
	}

	//뷰모델 AddTransition은 PostEdit(ArrayAdd)에서 대상 ID가 없는 트랜지션(NextState, Succeeded 등)을
	//OnStateCompleted → GotoState Root로 덮어쓰므로 스테이트 빌더 API로 직접 추가
	const FScopedTransaction ScopedTransaction(LOCTEXT("AddTransitionTransaction", "Add Transition"));
	State->Modify();

	FStateTreeTransition& NewTransition = State->AddTransition(TriggerValue, EventTagValue, TypeValue, TypeValue == EStateTreeTransitionType::GotoState ? TargetState : nullptr);
	NewTransition.Priority = PriorityValue;
	const FGuid NewTransitionID = NewTransition.ID;

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
	{
		ViewModel->GetOnStateNodesChanged().Broadcast(State);
	}

	DEBUG_LOG(TEXT("AddTransition: %s %s -> %s on %s"), *Trigger, *EventTag, *TransitionType, *State->Name.ToString());
	return NewTransitionID;
}

FString UActionPracticeStateTreeEditLibrary::SetTransitionTarget(UStateTree* StateTree, UStateTreeState* State, FGuid TransitionID, const FString& TransitionType, UStateTreeState* TargetState)
{
	FString Error = APStateTreeEdit::ValidateState(StateTree, State);

	if (Error.IsEmpty() && TargetState)
	{
		Error = APStateTreeEdit::ValidateState(StateTree, TargetState);
	}

	if (!Error.IsEmpty())
	{
		return Error;
	}

	FStateTreeTransition* Transition = APStateTreeEdit::FindTransition(State, TransitionID);

	if (!Transition)
	{
		return FString::Printf(TEXT("Transition %s not found on state %s"), *APStateTreeEdit::GuidToString(TransitionID), *State->Name.ToString());
	}

	EStateTreeTransitionType TypeValue = EStateTreeTransitionType::GotoState;

	if (!APStateTreeEdit::ParseEnum(TransitionType, TypeValue))
	{
		return FString::Printf(TEXT("Unknown TransitionType '%s' (valid: %s)"), *TransitionType, *APStateTreeEdit::EnumNameList<EStateTreeTransitionType>());
	}

	if (TypeValue == EStateTreeTransitionType::GotoState && !TargetState)
	{
		return TEXT("GotoState transition requires TargetState");
	}

	const FScopedTransaction ScopedTransaction(LOCTEXT("SetTransitionTargetTransaction", "Set Transition Target"));
	State->Modify();

	Transition->State = TypeValue == EStateTreeTransitionType::GotoState ? TargetState->GetLinkToState() : FStateTreeStateLink(TypeValue);

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
	{
		ViewModel->GetOnStateNodesChanged().Broadcast(State);
	}

	return FString();
}

#pragma endregion

#pragma region "Bindings"

FString UActionPracticeStateTreeEditLibrary::SetPropertyBinding(UStateTree* StateTree, UStateTreeState* State, FGuid TargetNodeID, const FString& TargetPropertyPath, const FString& SourceStruct, const FString& SourcePropertyPath)
{
	FStateTreeEditorNode* TargetNode = nullptr;
	FString Error = APStateTreeEdit::ValidateBindingTarget(StateTree, State, TargetNodeID, TargetNode);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	//대상 노드 실행 경로에서 접근 가능한 소스만 허용 (에디터 바인딩 메뉴와 같은 범위)
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> SourceDescs;
	EditorData->GetBindableStructs(TargetNodeID, SourceDescs);

	FPropertyBindingPath TargetPath(TargetNodeID);
	const FProperty* TargetProperty = nullptr;
	Error = APStateTreeEdit::ResolveBindingPath(EditorData, TargetPath, TargetPropertyPath, TargetProperty);

	if (!Error.IsEmpty())
	{
		return TEXT("Target: ") + Error;
	}

	//이름이 같은 소스가 여럿일 수 있음 (노드 인스턴스 데이터와 노드 구조체, 글로벌/스테이트 파라미터)
	//이름이 일치하는 후보 중 소스 경로가 실제로 해석되는 것만 남김
	struct FSourceCandidate
	{
		const FPropertyBindingBindableStructDescriptor* Desc = nullptr;
		FPropertyBindingPath Path;
		const FProperty* LeafProperty = nullptr;
	};

	TArray<FSourceCandidate> Resolved;
	TArray<FString> AvailableNames;
	TArray<FString> ResolveErrors;

	for (const TInstancedStruct<FPropertyBindingBindableStructDescriptor>& Desc : SourceDescs)
	{
		if (!Desc.IsValid())
		{
			continue;
		}

		const FPropertyBindingBindableStructDescriptor& SourceDesc = Desc.Get<FPropertyBindingBindableStructDescriptor>();
		AvailableNames.Add(APStateTreeEdit::DescribeBindableStruct(SourceDesc));

		if (!APStateTreeEdit::MatchesSourceStruct(SourceDesc, SourceStruct))
		{
			continue;
		}

		FSourceCandidate Candidate;
		Candidate.Desc = &SourceDesc;
		Candidate.Path = FPropertyBindingPath(SourceDesc.ID);
		const FString CandidateError = APStateTreeEdit::ResolveBindingPath(EditorData, Candidate.Path, SourcePropertyPath, Candidate.LeafProperty, true, SourceDesc.Struct);

		if (CandidateError.IsEmpty())
		{
			Resolved.Add(MoveTemp(Candidate));
		}
		else
		{
			ResolveErrors.Add(FString::Printf(TEXT("%s: %s"), *APStateTreeEdit::DescribeBindableStruct(SourceDesc), *CandidateError));
		}
	}

	if (Resolved.IsEmpty())
	{
		if (!ResolveErrors.IsEmpty())
		{
			return TEXT("Source: ") + FString::Join(ResolveErrors, TEXT("; "));
		}

		return FString::Printf(TEXT("Source '%s' is not accessible from node %s (available: %s)"), *SourceStruct, *APStateTreeEdit::GuidToString(TargetNodeID), *FString::Join(AvailableNames, TEXT("; ")));
	}

	if (Resolved.Num() > 1)
	{
		TArray<FString> MatchNames;

		for (const FSourceCandidate& Candidate : Resolved)
		{
			MatchNames.Add(APStateTreeEdit::DescribeBindableStruct(*Candidate.Desc));
		}

		return FString::Printf(TEXT("Source '%s' is ambiguous; pass its id instead (%s)"), *SourceStruct, *FString::Join(MatchNames, TEXT("; ")));
	}

	const FSourceCandidate& Source = Resolved[0];
	const FPropertyBindingPath& SourcePath = Source.Path;

	//빈 소스 경로는 구조체·컨텍스트 객체 통째 바인딩
	const bool bCompatible = Source.LeafProperty
		? UE::PropertyBinding::GetPropertyCompatibility(Source.LeafProperty, TargetProperty) != UE::PropertyBinding::EPropertyCompatibility::Incompatible
		: APStateTreeEdit::IsWholeStructCompatible(Source.Desc->Struct, TargetProperty);

	if (!bCompatible)
	{
		const FString SourceType = Source.LeafProperty ? Source.LeafProperty->GetCPPType() : GetNameSafe(Source.Desc->Struct);
		return FString::Printf(TEXT("Incompatible types: source %s.%s (%s) -> target %s (%s)"), *Source.Desc->Name.ToString(), *SourcePropertyPath, *SourceType, *TargetPropertyPath, *TargetProperty->GetCPPType());
	}

	const FScopedTransaction ScopedTransaction(LOCTEXT("SetPropertyBindingTransaction", "Set StateTree Property Binding"));
	EditorData->Modify();

	//같은 대상 경로의 기존 바인딩은 AddBinding 내부에서 제거 후 교체
	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	EditorData->OnPropertyBindingChanged(SourcePath, TargetPath);

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (State)
	{
		if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
		{
			ViewModel->GetOnStateNodesChanged().Broadcast(State);
		}
	}

	DEBUG_LOG(TEXT("SetPropertyBinding: %s.%s <- %s.%s"), *TargetNode->GetName().ToString(), *TargetPropertyPath, *Source.Desc->Name.ToString(), *SourcePropertyPath);
	return FString();
}

FString UActionPracticeStateTreeEditLibrary::RemovePropertyBinding(UStateTree* StateTree, UStateTreeState* State, FGuid TargetNodeID, const FString& TargetPropertyPath)
{
	FStateTreeEditorNode* TargetNode = nullptr;
	const FString Error = APStateTreeEdit::ValidateBindingTarget(StateTree, State, TargetNodeID, TargetNode);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	//SetPropertyBinding과 같은 정규화 경로로 비교 (세그먼트 인스턴스 타입 포함)
	//프로퍼티 이름 변경 등으로 해석이 안 되면 남은 바인딩 정리를 위해 문자열 경로로 폴백
	FPropertyBindingPath TargetPath(TargetNodeID);
	const FProperty* TargetProperty = nullptr;

	if (!APStateTreeEdit::ResolveBindingPath(EditorData, TargetPath, TargetPropertyPath, TargetProperty).IsEmpty())
	{
		TargetPath = FPropertyBindingPath(TargetNodeID);

		if (TargetPropertyPath.IsEmpty() || !TargetPath.FromString(TargetPropertyPath))
		{
			return FString::Printf(TEXT("Invalid property path '%s'"), *TargetPropertyPath);
		}
	}

	//변경이 없을 때 빈 Undo 항목이 남지 않도록 존재 여부를 먼저 확인 (RemovePropertyBinding의 Includes 검색과 동일 기준)
	bool bHasBinding = false;

	EditorData->GetPropertyEditorBindings()->ForEachBinding([&TargetPath, &bHasBinding](const FPropertyBindingBinding& Binding)
	{
		bHasBinding |= Binding.GetTargetPath().Includes(TargetPath);
	});

	if (!bHasBinding)
	{
		return FString::Printf(TEXT("No binding on %s.%s"), *TargetNode->GetName().ToString(), *TargetPropertyPath);
	}

	const FScopedTransaction ScopedTransaction(LOCTEXT("RemovePropertyBindingTransaction", "Remove StateTree Property Binding"));
	EditorData->Modify();
	EditorData->RemovePropertyBinding(TargetPath);

	UStateTreeEditingSubsystem::MarkAsModified(StateTree);

	if (State)
	{
		if (TSharedPtr<FStateTreeViewModel> ViewModel = APStateTreeEdit::GetViewModel(StateTree))
		{
			ViewModel->GetOnStateNodesChanged().Broadcast(State);
		}
	}

	DEBUG_LOG(TEXT("RemovePropertyBinding: %s.%s"), *TargetNode->GetName().ToString(), *TargetPropertyPath);
	return FString();
}

FString UActionPracticeStateTreeEditLibrary::GetBindingSources(UStateTree* StateTree, UStateTreeState* State, FGuid TargetNodeID)
{
	FStateTreeEditorNode* TargetNode = nullptr;
	const FString Error = APStateTreeEdit::ValidateBindingTarget(StateTree, State, TargetNodeID, TargetNode);

	if (!Error.IsEmpty())
	{
		return Error;
	}

	const UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);
	TArray<FString> Lines;

	//데이터 조회 실패 시 빈 뷰 → 프로퍼티 목록 "(no data)"
	FPropertyBindingDataView TargetView;
	EditorData->GetBindingDataViewByID(TargetNodeID, TargetView);
	Lines.Add(FString::Printf(TEXT("Target %s: %s"), *APStateTreeEdit::DescribeNode(*TargetNode), *APStateTreeEdit::DescribeStructProperties(TargetView.GetStruct())));

	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> SourceDescs;
	EditorData->GetBindableStructs(TargetNodeID, SourceDescs);

	for (const TInstancedStruct<FPropertyBindingBindableStructDescriptor>& Desc : SourceDescs)
	{
		if (!Desc.IsValid())
		{
			continue;
		}

		const FPropertyBindingBindableStructDescriptor& SourceDesc = Desc.Get<FPropertyBindingBindableStructDescriptor>();

		//프로퍼티 백(파라미터)은 실제 값 구조체에서 프로퍼티를 읽어야 함
		FPropertyBindingDataView SourceView;
		const UStruct* SourceStruct = EditorData->GetBindingDataViewByID(SourceDesc.ID, SourceView) && SourceView.IsValid() ? SourceView.GetStruct() : SourceDesc.Struct.Get();

		Lines.Add(FString::Printf(TEXT("Source %s: %s"), *APStateTreeEdit::DescribeBindableStruct(SourceDesc), *APStateTreeEdit::DescribeStructProperties(SourceStruct)));
	}

	return FString::Join(Lines, TEXT("\n"));
}

namespace APStateTreeEdit
{
	//엔진이 루트 파라미터 가변 접근자를 제공하지 않아 리플렉션으로 접근 (엔진 버전 의존, 추가는 엔진 API CreateRootProperties 사용)
	FInstancedPropertyBag* GetMutableRootParameterBag(UStateTreeEditorData* EditorData)
	{
		const FStructProperty* BagProperty = FindFProperty<FStructProperty>(UStateTreeEditorData::StaticClass(), TEXT("RootParameterPropertyBag"));

		if (!BagProperty || BagProperty->Struct != FInstancedPropertyBag::StaticStruct())
		{
			return nullptr;
		}

		return BagProperty->ContainerPtrToValuePtr<FInstancedPropertyBag>(EditorData);
	}
}

FString UActionPracticeStateTreeEditLibrary::SetRootParameter(UStateTree* StateTree, FName ParameterName, const FString& Type, const FString& ValueText)
{
	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	if (!EditorData)
	{
		return TEXT("StateTree or its editor data is null");
	}

	if (ParameterName.IsNone())
	{
		return TEXT("ParameterName is empty");
	}

	FInstancedPropertyBag* Bag = APStateTreeEdit::GetMutableRootParameterBag(EditorData);

	if (!Bag)
	{
		return TEXT("RootParameterPropertyBag not found on UStateTreeEditorData");
	}

	const FPropertyBagPropertyDesc* ExistingDesc = Bag->FindPropertyDescByName(ParameterName);
	const bool bExists = ExistingDesc != nullptr;
	EPropertyBagPropertyType ValueType = bExists ? ExistingDesc->ValueType : EPropertyBagPropertyType::None;

	//타입 오브젝트가 필요 없는 단일 값 단순 타입만 허용 (기존 파라미터 포함)
	static const TSet<EPropertyBagPropertyType> SupportedTypes =
	{
		EPropertyBagPropertyType::Bool, EPropertyBagPropertyType::Byte, EPropertyBagPropertyType::Int32, EPropertyBagPropertyType::Int64,
		EPropertyBagPropertyType::Float, EPropertyBagPropertyType::Double, EPropertyBagPropertyType::Name, EPropertyBagPropertyType::String,
		EPropertyBagPropertyType::Text,
	};

	if (bExists)
	{
		if (!SupportedTypes.Contains(ValueType) || !ExistingDesc->ContainerTypes.IsEmpty())
		{
			return FString::Printf(TEXT("Existing parameter %s has unsupported type %s%s (edit it in the StateTree editor)"),
				*ParameterName.ToString(), *APStateTreeEdit::EnumToString(ValueType), ExistingDesc->ContainerTypes.IsEmpty() ? TEXT("") : TEXT(" container"));
		}
	}
	else if (!APStateTreeEdit::ParseEnum(Type, ValueType) || !SupportedTypes.Contains(ValueType))
	{
		return FString::Printf(TEXT("Unsupported Type '%s' (valid: Bool, Byte, Int32, Int64, Float, Double, Name, String, Text)"), *Type);
	}

	//숫자·Bool만 앞뒤 공백 제거 (Name·String·Text는 원문 유지)
	const bool bNumericOrBool = ValueType != EPropertyBagPropertyType::Name && ValueType != EPropertyBagPropertyType::String && ValueType != EPropertyBagPropertyType::Text;
	const FString ValueToSet = bNumericOrBool ? ValueText.TrimStartAndEnd() : ValueText;

	//변경 전에 값 검증 (생성 후 실패로 기본값 파라미터만 남는 상황 방지)
	if (!ValueText.IsEmpty())
	{
		//숫자·Bool 텍스트 임포트는 잘못된 문자열·범위 초과도 0/false 또는 래핑 값으로 성공 처리하므로 직접 검사
		bool bValidFormat = true;

		switch (ValueType)
		{
		case EPropertyBagPropertyType::Byte:
		case EPropertyBagPropertyType::Int32:
		{
			const int64 MinValue = ValueType == EPropertyBagPropertyType::Byte ? 0 : MIN_int32;
			const int64 MaxValue = ValueType == EPropertyBagPropertyType::Byte ? MAX_uint8 : MAX_int32;
			const int64 ParsedValue = FCString::Atoi64(*ValueToSet);
			bValidFormat = FDefaultValueHelper::IsStringValidInteger(ValueToSet) && ParsedValue >= MinValue && ParsedValue <= MaxValue;
			break;
		}
		case EPropertyBagPropertyType::Int64:
			bValidFormat = FDefaultValueHelper::IsStringValidInteger(ValueToSet);
			break;
		case EPropertyBagPropertyType::Float:
		case EPropertyBagPropertyType::Double:
			bValidFormat = FDefaultValueHelper::IsStringValidFloat(ValueToSet) || FDefaultValueHelper::IsStringValidInteger(ValueToSet);
			break;
		case EPropertyBagPropertyType::Bool:
			bValidFormat = ValueToSet.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueToSet.Equals(TEXT("false"), ESearchCase::IgnoreCase)
				|| ValueToSet == TEXT("1") || ValueToSet == TEXT("0");
			break;
		default:
			break;
		}

		if (!bValidFormat)
		{
			return FString::Printf(TEXT("Invalid value '%s' for %s parameter %s"), *ValueText, *APStateTreeEdit::EnumToString(ValueType), *ParameterName.ToString());
		}

		//형식 외 실패도 실제 Bag 변경 전에 확인
		FInstancedPropertyBag ValidationBag;
		ValidationBag.AddProperty(ParameterName, ValueType);
		const EPropertyBagResult Result = ValidationBag.SetValueSerializedString(ParameterName, ValueToSet);

		if (Result != EPropertyBagResult::Success)
		{
			return FString::Printf(TEXT("Invalid value '%s' for %s parameter %s (%s)"), *ValueText, *APStateTreeEdit::EnumToString(ValueType), *ParameterName.ToString(), *APStateTreeEdit::EnumToString(Result));
		}
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("SetRootParameterTransaction", "Set StateTree Root Parameter"));
	EditorData->Modify();

	if (!bExists)
	{
		UE::PropertyBinding::FPropertyCreationDescriptor CreationDesc;
		CreationDesc.PropertyDesc = FPropertyBagPropertyDesc(ParameterName, ValueType);
		EditorData->CreateRootProperties(MakeArrayView(&CreationDesc, 1));

		if (!Bag->FindPropertyDescByName(ParameterName))
		{
			ScopedTransaction.Cancel();
			return FString::Printf(TEXT("Failed to create parameter %s"), *ParameterName.ToString());
		}
	}

	if (!ValueText.IsEmpty())
	{
		const EPropertyBagResult Result = Bag->SetValueSerializedString(ParameterName, ValueToSet);

		if (Result != EPropertyBagResult::Success)
		{
			ScopedTransaction.Cancel();
			return FString::Printf(TEXT("Failed to set %s to '%s' (%s)"), *ParameterName.ToString(), *ValueText, *APStateTreeEdit::EnumToString(Result));
		}
	}

	//디테일 패널 편집(PostEditChangeChainProperty)과 동일한 통지: 바인딩 갱신·에디터 디테일 새로고침 후 공개 변경 표시
	UE::StateTree::Delegates::OnParametersChanged.Broadcast(*StateTree);
	UStateTreeEditingSubsystem::MarkAsPubliclyModified(StateTree);

	DEBUG_LOG(TEXT("SetRootParameter: %s = %s"), *ParameterName.ToString(), *ValueText);
	return FString();
}

FString UActionPracticeStateTreeEditLibrary::RemoveRootParameter(UStateTree* StateTree, FName ParameterName)
{
	UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	if (!EditorData)
	{
		return TEXT("StateTree or its editor data is null");
	}

	FInstancedPropertyBag* Bag = APStateTreeEdit::GetMutableRootParameterBag(EditorData);

	if (!Bag)
	{
		return TEXT("RootParameterPropertyBag not found on UStateTreeEditorData");
	}

	if (!Bag->FindPropertyDescByName(ParameterName))
	{
		return FString::Printf(TEXT("Parameter %s not found"), *ParameterName.ToString());
	}

	//삭제 시 끊어질 바인딩이 남지 않도록 사용 중이면 거부 (바인딩 먼저 제거)
	const FGuid RootParametersID = EditorData->GetRootParametersGuid();
	TArray<FString> UsedBy;

	EditorData->GetPropertyEditorBindings()->ForEachBinding([EditorData, RootParametersID, ParameterName, &UsedBy](const FPropertyBindingBinding& Binding)
	{
		const FPropertyBindingPath& SourcePath = Binding.GetSourcePath();

		if (SourcePath.GetStructID() == RootParametersID && SourcePath.NumSegments() > 0 && SourcePath.GetSegment(0).GetName() == ParameterName)
		{
			const FPropertyBindingPath& TargetPath = Binding.GetTargetPath();
			UsedBy.Add(FString::Printf(TEXT("%s(%s).%s"), *APStateTreeEdit::GetBindableStructName(EditorData, TargetPath.GetStructID()),
				*APStateTreeEdit::GuidToString(TargetPath.GetStructID()), *TargetPath.ToString()));
		}
	});

	if (!UsedBy.IsEmpty())
	{
		return FString::Printf(TEXT("Parameter %s is bound by: %s"), *ParameterName.ToString(), *FString::Join(UsedBy, TEXT(", ")));
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("RemoveRootParameterTransaction", "Remove StateTree Root Parameter"));
	EditorData->Modify();

	if (Bag->RemovePropertyByName(ParameterName) != EPropertyBagAlterationResult::Success)
	{
		ScopedTransaction.Cancel();
		return FString::Printf(TEXT("Failed to remove parameter %s"), *ParameterName.ToString());
	}

	UE::StateTree::Delegates::OnParametersChanged.Broadcast(*StateTree);
	UStateTreeEditingSubsystem::MarkAsPubliclyModified(StateTree);

	DEBUG_LOG(TEXT("RemoveRootParameter: %s"), *ParameterName.ToString());
	return FString();
}

FString UActionPracticeStateTreeEditLibrary::DescribeBindings(UStateTree* StateTree, UStateTreeState* State)
{
	const UStateTreeEditorData* EditorData = APStateTreeEdit::GetEditorData(StateTree);

	if (!EditorData)
	{
		return TEXT("StateTree or its editor data is null");
	}

	if (State)
	{
		const FString Error = APStateTreeEdit::ValidateState(StateTree, State);

		if (!Error.IsEmpty())
		{
			return Error;
		}
	}

	TArray<FString> Lines;

	EditorData->GetPropertyEditorBindings()->ForEachBinding([EditorData, State, &Lines](const FPropertyBindingBinding& Binding)
	{
		const FPropertyBindingPath& TargetPath = Binding.GetTargetPath();
		const FPropertyBindingPath& SourcePath = Binding.GetSourcePath();

		if (State && !APStateTreeEdit::FindEditorNode(State, TargetPath.GetStructID()))
		{
			return;
		}

		Lines.Add(FString::Printf(TEXT("%s(%s).%s <- %s.%s"),
			*APStateTreeEdit::GetBindableStructName(EditorData, TargetPath.GetStructID()),
			*APStateTreeEdit::GuidToString(TargetPath.GetStructID()),
			*TargetPath.ToString(),
			*APStateTreeEdit::GetBindableStructName(EditorData, SourcePath.GetStructID()),
			*SourcePath.ToString()));
	});

	return Lines.IsEmpty() ? TEXT("(no bindings)") : FString::Join(Lines, TEXT("\n"));
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
