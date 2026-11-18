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

FString UActionPracticeEditorScriptLibrary::SetMontageSegments(UAnimMontage* Montage, int32 SlotTrackIndex, const TArray<UAnimSequenceBase*>& Animations, const TArray<float>& AnimStartTimes, const TArray<float>& AnimEndTimes, const TArray<float>& PlayRates)
{
	if (!Montage)
	{
		return TEXT("Montage is null");
	}

	if (!Montage->SlotAnimTracks.IsValidIndex(SlotTrackIndex))
	{
		return FString::Printf(TEXT("Slot track index %d is out of range (%d tracks)"), SlotTrackIndex, Montage->SlotAnimTracks.Num());
	}

	const int32 NumSegments = Animations.Num();

	if (NumSegments == 0 || AnimStartTimes.Num() != NumSegments || AnimEndTimes.Num() != NumSegments || PlayRates.Num() != NumSegments)
	{
		return TEXT("Animations, AnimStartTimes, AnimEndTimes and PlayRates must be non-empty and have the same length");
	}

	//새 트랙을 먼저 구성해 검증 실패 시 몽타주를 건드리지 않음
	FAnimTrack NewTrack;
	float NextStartPos = 0.0f;

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		UAnimSequenceBase* Animation = Animations[Index];
		FText Reason;

		//몽타주 중첩, 길이 0, 가산 타입 불일치 등 엔진 규칙 검사
		if (!NewTrack.IsValidToAdd(Animation, &Reason))
		{
			return FString::Printf(TEXT("Segment %d: %s"), Index, Reason.IsEmpty() ? TEXT("animation is not valid for a montage") : *Reason.ToString());
		}

		const USkeleton* Skeleton = Montage->GetSkeleton();

		if (Skeleton && !Skeleton->IsCompatibleForEditor(Animation->GetSkeleton()))
		{
			return FString::Printf(TEXT("Segment %d: %s uses a skeleton incompatible with the montage"), Index, *GetNameSafe(Animation));
		}

		const float AnimLength = Animation->GetPlayLength();

		if (AnimStartTimes[Index] < 0.0f || AnimEndTimes[Index] > AnimLength + UE_KINDA_SMALL_NUMBER || AnimStartTimes[Index] >= AnimEndTimes[Index])
		{
			return FString::Printf(TEXT("Segment %d: range %.3f-%.3f is invalid for %s (length %.3f)"), Index, AnimStartTimes[Index], AnimEndTimes[Index], *GetNameSafe(Animation), AnimLength);
		}

		//역재생은 노티파이·루트 모션 해석이 달라 지원하지 않음
		if (PlayRates[Index] <= 0.0f)
		{
			return FString::Printf(TEXT("Segment %d: play rate must be positive (got %.3f)"), Index, PlayRates[Index]);
		}

		FAnimSegment Segment;
		Segment.SetAnimReference(Animation, true);
		Segment.AnimStartTime = AnimStartTimes[Index];
		Segment.AnimEndTime = FMath::Min(AnimEndTimes[Index], AnimLength);
		Segment.AnimPlayRate = PlayRates[Index];
		Segment.LoopingCount = 1;
		Segment.StartPos = NextStartPos;
		NextStartPos += Segment.GetLength();
		NewTrack.AnimSegments.Add(Segment);
	}

	//교체 전 이 슬롯에 링크된 섹션·노티파이의 몽타주 절대 시간 저장 (교체 후 같은 시간으로 재링크)
	TArray<float> SectionTimes;
	TArray<float> NotifyTimes;
	TArray<float> NotifyEndTimes;
	float LatestLinkedTime = 0.0f;

	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		SectionTimes.Add(Section.GetTime());

		if (Section.GetSlotIndex() == SlotTrackIndex)
		{
			LatestLinkedTime = FMath::Max(LatestLinkedTime, Section.GetTime());
		}
	}

	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		NotifyTimes.Add(Notify.GetTime());
		NotifyEndTimes.Add(Notify.GetTime() + Notify.GetDuration());

		if (Notify.GetSlotIndex() == SlotTrackIndex)
		{
			LatestLinkedTime = FMath::Max(LatestLinkedTime, Notify.GetTime() + Notify.GetDuration());
		}
	}

	//몽타주 에디터의 ClampToEndTime처럼 이벤트를 몰래 당기지 않고 거부
	if (NextStartPos + UE_KINDA_SMALL_NUMBER < LatestLinkedTime)
	{
		return FString::Printf(TEXT("New track length %.3f is shorter than linked sections/notifies (latest %.3f)"), NextStartPos, LatestLinkedTime);
	}

	Montage->Modify();
	Montage->SlotAnimTracks[SlotTrackIndex].AnimTrack = NewTrack;

	for (int32 Index = 0; Index < Montage->CompositeSections.Num(); ++Index)
	{
		FCompositeSection& Section = Montage->CompositeSections[Index];

		if (Section.GetSlotIndex() == SlotTrackIndex)
		{
			Section.Link(Montage, SectionTimes[Index], SlotTrackIndex);
		}
	}

	for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
	{
		FAnimNotifyEvent& Notify = Montage->Notifies[Index];

		if (Notify.GetSlotIndex() == SlotTrackIndex)
		{
			Notify.Link(Montage, NotifyTimes[Index], SlotTrackIndex);
			Notify.EndLink.Link(Montage, NotifyEndTimes[Index], SlotTrackIndex);
		}
	}

	//몽타주 에디터 RecalculateSequenceLength와 같은 순서 (재귀 참조 검사 → 길이 → 링크·트리거 오프셋)
	Montage->InvalidateRecursiveAsset();
	Montage->SetCompositeLength(Montage->CalculateSequenceLength());
	Montage->UpdateLinkableElements();
	Montage->PostEditChange();
	Montage->MarkPackageDirty();

	DEBUG_LOG(TEXT("SetMontageSegments: %s[%d] -> %d segments, length %.3f"), *GetNameSafe(Montage), SlotTrackIndex, NumSegments, Montage->GetPlayLength());
	return FString();
}
