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
