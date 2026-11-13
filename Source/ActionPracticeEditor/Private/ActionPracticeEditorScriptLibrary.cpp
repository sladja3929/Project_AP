#include "ActionPracticeEditorScriptLibrary.h"

#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "Logging/TokenizedMessage.h"
#include "Animation/AnimMontage.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogActionPracticeEditorScriptLibrary, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogActionPracticeEditorScriptLibrary, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

TArray<FString> UActionPracticeEditorScriptLibrary::CompileStateTree(UStateTree* StateTree, bool& bOutSucceeded)
{
	TArray<FString> Messages;
	bOutSucceeded = false;

	if (!StateTree)
	{
		Messages.Add(TEXT("StateTree is null"));
		return Messages;
	}

	FStateTreeCompilerLog Log;
	bOutSucceeded = UStateTreeEditingSubsystem::CompileStateTree(StateTree, Log);

	for (const TSharedRef<FTokenizedMessage>& Message : Log.ToTokenizedMessages())
	{
		Messages.Add(Message->ToText().ToString());
	}

	DEBUG_LOG(TEXT("CompileStateTree: %s -> %s (%d messages)"), *GetNameSafe(StateTree), bOutSucceeded ? TEXT("Succeeded") : TEXT("Failed"), Messages.Num());
	return Messages;
}

bool UActionPracticeEditorScriptLibrary::StateTreeNeedsRecompile(UStateTree* StateTree)
{
	if (!StateTree)
	{
		return false;
	}

	return UStateTreeEditingSubsystem::NeedsRecompile(StateTree);
}

bool UActionPracticeEditorScriptLibrary::SetMontageSlotName(UAnimMontage* Montage, int32 SlotTrackIndex, FName SlotName)
{
	if (!Montage || !Montage->SlotAnimTracks.IsValidIndex(SlotTrackIndex) || SlotName.IsNone())
	{
		return false;
	}

	//스켈레톤 Anim Slot Manager에 등록되지 않은 슬롯은 ABP Slot 노드와 매칭되지 않음
	const USkeleton* Skeleton = Montage->GetSkeleton();

	if (!Skeleton || !Skeleton->ContainsSlotName(SlotName))
	{
		DEBUG_LOG(TEXT("SetMontageSlotName: slot %s is not registered on skeleton %s"), *SlotName.ToString(), *GetNameSafe(Skeleton));
		return false;
	}

	Montage->Modify();
	Montage->SlotAnimTracks[SlotTrackIndex].SlotName = SlotName;
	Montage->PostEditChange();
	Montage->MarkPackageDirty();

	DEBUG_LOG(TEXT("SetMontageSlotName: %s[%d] -> %s"), *GetNameSafe(Montage), SlotTrackIndex, *SlotName.ToString());
	return true;
}

void UActionPracticeEditorScriptLibrary::GetMontageSections(UAnimMontage* Montage, TArray<FName>& OutNames, TArray<float>& OutStartTimes, TArray<FName>& OutNextNames)
{
	OutNames.Reset();
	OutStartTimes.Reset();
	OutNextNames.Reset();

	if (!Montage)
	{
		return;
	}

	TArray<const FCompositeSection*> Sorted;

	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		Sorted.Add(&Section);
	}

	Sorted.Sort([](const FCompositeSection& A, const FCompositeSection& B) { return A.GetTime() < B.GetTime(); });

	for (const FCompositeSection* Section : Sorted)
	{
		OutNames.Add(Section->SectionName);
		OutStartTimes.Add(Section->GetTime());
		OutNextNames.Add(Section->NextSectionName);
	}
}

FString UActionPracticeEditorScriptLibrary::SetMontageSections(UAnimMontage* Montage, const TArray<FName>& Names, const TArray<float>& StartTimes, const TArray<FName>& NextNames)
{
	if (!Montage)
	{
		return TEXT("Montage is null");
	}

	if (Names.IsEmpty() || Names.Num() != StartTimes.Num() || Names.Num() != NextNames.Num())
	{
		return TEXT("Names, StartTimes and NextNames must be non-empty and have the same length");
	}

	const float PlayLength = Montage->GetPlayLength();
	TSet<FName> NameSet;
	TSet<float> StartTimeSet;
	TArray<int32> Order;
	bool bHasZeroSection = false;

	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		if (Names[Index].IsNone())
		{
			return FString::Printf(TEXT("Section %d has no name"), Index);
		}

		if (NameSet.Contains(Names[Index]))
		{
			return FString::Printf(TEXT("Duplicate section name %s"), *Names[Index].ToString());
		}

		if (StartTimes[Index] < 0.0f || StartTimes[Index] >= PlayLength)
		{
			return FString::Printf(TEXT("Section %s start time %.3f is outside [0, %.3f)"), *Names[Index].ToString(), StartTimes[Index], PlayLength);
		}

		//시작 시간이 같으면 길이 0 섹션이 생기고 정렬 순서도 비결정적
		if (StartTimeSet.Contains(StartTimes[Index]))
		{
			return FString::Printf(TEXT("Duplicate section start time %.3f"), StartTimes[Index]);
		}

		bHasZeroSection |= FMath::IsNearlyZero(StartTimes[Index]);
		NameSet.Add(Names[Index]);
		StartTimeSet.Add(StartTimes[Index]);
		Order.Add(Index);
	}

	//첫 섹션 이전 구간은 어떤 섹션에도 속하지 않아 재생 흐름에서 빠짐
	if (!bHasZeroSection)
	{
		return TEXT("A section starting at 0 is required");
	}

	for (const FName& NextName : NextNames)
	{
		if (!NextName.IsNone() && !NameSet.Contains(NextName))
		{
			return FString::Printf(TEXT("Next section %s does not exist"), *NextName.ToString());
		}
	}

	//SortAnimCompositeSectionByPos는 엔진 모듈 외부에 노출되지 않아 직접 시간순 정렬
	Order.Sort([&StartTimes](int32 A, int32 B) { return StartTimes[A] < StartTimes[B]; });

	Montage->Modify();
	Montage->CompositeSections.Reset();

	for (const int32 Index : Order)
	{
		FCompositeSection Section;
		Section.SectionName = Names[Index];
		Section.NextSectionName = NextNames[Index];
		Section.Link(Montage, StartTimes[Index]);
		Montage->CompositeSections.Add(Section);
	}

	Montage->PostEditChange();
	Montage->MarkPackageDirty();

	DEBUG_LOG(TEXT("SetMontageSections: %s -> %d sections"), *GetNameSafe(Montage), Montage->CompositeSections.Num());
	return FString();
}
