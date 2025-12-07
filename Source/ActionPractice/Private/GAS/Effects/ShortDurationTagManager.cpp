#include "GAS/Effects/ShortDurationTagManager.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogShortDurationTagManager, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogShortDurationTagManager, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UShortDurationTagManager::UShortDurationTagManager()
{
	UpdateInterval = 0.05f;
}

void UShortDurationTagManager::Initialize(UAbilitySystemComponent* InOwnerASC)
{
	if (!InOwnerASC)
	{
		DEBUG_LOG(TEXT("Initialize failed: Invalid ASC"));
		return;
	}

	OwnerASC = InOwnerASC;
	ActiveTags.Empty();
	
	DEBUG_LOG(TEXT("ShortDurationTagManager initialized for ASC: %s"), *GetNameSafe(InOwnerASC->GetOwner()));
}

void UShortDurationTagManager::Cleanup()
{
	RemoveAllTags();
	StopUpdateTimer();
	OwnerASC = nullptr;
	
	DEBUG_LOG(TEXT("ShortDurationTagManager cleaned up"));
}

void UShortDurationTagManager::ApplyTag(
	UAbilitySystemComponent* ASC,
	const FGameplayTag& Tag,
	float Duration,
	bool bIsStack)
{
	if (!ASC || !Tag.IsValid() || Duration <= 0.0f)
	{
		DEBUG_LOG(TEXT("ApplyTag failed: Invalid parameters"));
		return;
	}

	//ASC 설정
	if (!OwnerASC.IsValid())
	{
		OwnerASC = ASC;
	}
	else if (OwnerASC != ASC)
	{
		DEBUG_LOG(TEXT("Warning: Different ASC provided than initialized"));
		return;
	}

	UWorld* World = ASC->GetWorld();
	if (!World)
	{
		DEBUG_LOG(TEXT("ApplyTag failed: No World"));
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	float NewEndTime = CurrentTime + Duration;

	//기존 태그 처리
	if (FShortDurationTagInfo* ExistingInfo = ActiveTags.Find(Tag))
	{
		if (bIsStack)
		{
			//스택 모드: 시간 누적
			float RemainingTime = FMath::Max(0.0f, ExistingInfo->EndTime - CurrentTime);
			NewEndTime = CurrentTime + RemainingTime + Duration;
			DEBUG_LOG(TEXT("Stacking tag %s: Remaining(%.3f) + New(%.3f) = Total(%.3f)"),
				*Tag.ToString(), RemainingTime, Duration, NewEndTime - CurrentTime);
		}
		else
		{
			//비스택 모드: 더 긴 시간 적용
			if (NewEndTime <= ExistingInfo->EndTime)
			{
				DEBUG_LOG(TEXT("Skip tag %s: New duration(%.3f) <= Remaining(%.3f)"),
					*Tag.ToString(), Duration, ExistingInfo->EndTime - CurrentTime);
				return;
			}
			DEBUG_LOG(TEXT("Extending tag %s: New duration(%.3f) > Remaining(%.3f)"),
				*Tag.ToString(), Duration, ExistingInfo->EndTime - CurrentTime);
		}

		//기존 정보 업데이트
		ExistingInfo->EndTime = NewEndTime;
		ExistingInfo->bIsStacking = bIsStack;
	}
	else
	{
		//새 태그 추가
		FShortDurationTagInfo NewInfo;
		NewInfo.Tag = Tag;
		NewInfo.EndTime = NewEndTime;
		NewInfo.bIsStacking = bIsStack;
		
		ActiveTags.Add(Tag, NewInfo);
		
		//ASC에 태그 추가
		ASC->AddLooseGameplayTag(Tag);
		
		DEBUG_LOG(TEXT("Added new tag %s for %.3f seconds (Stack: %s)"),
			*Tag.ToString(), Duration, bIsStack ? TEXT("true") : TEXT("false"));
	}

	//타이머 시작 (없으면)
	if (!UpdateTimerHandle.IsValid() && ActiveTags.Num() > 0)
	{
		StartUpdateTimer();
	}
}

void UShortDurationTagManager::RemoveTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag)
{
	if (!ASC || !Tag.IsValid())
	{
		return;
	}

	if (ActiveTags.Contains(Tag))
	{
		RemoveTagInternal(Tag);
		DEBUG_LOG(TEXT("Manually removed tag: %s"), *Tag.ToString());
	}
}

float UShortDurationTagManager::GetRemainingTime(const FGameplayTag& Tag) const
{
	if (!OwnerASC.IsValid())
	{
		return 0.0f;
	}

	if (const FShortDurationTagInfo* Info = ActiveTags.Find(Tag))
	{
		UWorld* World = OwnerASC->GetWorld();
		if (World)
		{
			float Remaining = Info->EndTime - World->GetTimeSeconds();
			return FMath::Max(0.0f, Remaining);
		}
	}

	return 0.0f;
}

void UShortDurationTagManager::RemoveAllTags()
{
	if (!OwnerASC.IsValid())
	{
		return;
	}

	for (const auto& Pair : ActiveTags)
	{
		OwnerASC->RemoveLooseGameplayTag(Pair.Key);
		DEBUG_LOG(TEXT("Removed tag on cleanup: %s"), *Pair.Key.ToString());
	}

	ActiveTags.Empty();
	StopUpdateTimer();
}

void UShortDurationTagManager::UpdateTags()
{
	if (!OwnerASC.IsValid())
	{
		StopUpdateTimer();
		return;
	}

	UWorld* World = OwnerASC->GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	TArray<FGameplayTag> TagsToRemove;

	//만료된 태그 찾기
	for (const auto& Pair : ActiveTags)
	{
		if (CurrentTime >= Pair.Value.EndTime)
		{
			TagsToRemove.Add(Pair.Key);
		}
	}

	//만료된 태그 제거
	for (const FGameplayTag& Tag : TagsToRemove)
	{
		RemoveTagInternal(Tag);
		DEBUG_LOG(TEXT("Tag expired and removed: %s"), *Tag.ToString());
	}

	//모든 태그가 제거되면 타이머 정지
	if (ActiveTags.Num() == 0)
	{
		StopUpdateTimer();
	}
}

void UShortDurationTagManager::RemoveTagInternal(const FGameplayTag& Tag)
{
	if (!OwnerASC.IsValid())
	{
		return;
	}

	if (ActiveTags.Remove(Tag) > 0)
	{
		OwnerASC->RemoveLooseGameplayTag(Tag);
	}
}

void UShortDurationTagManager::StartUpdateTimer()
{
	if (!OwnerASC.IsValid())
	{
		return;
	}

	UWorld* World = OwnerASC->GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		UpdateTimerHandle,
		this,
		&UShortDurationTagManager::UpdateTags,
		UpdateInterval,
		true //반복
	);
	
	DEBUG_LOG(TEXT("Update timer started"));
}

void UShortDurationTagManager::StopUpdateTimer()
{
	if (!UpdateTimerHandle.IsValid())
	{
		return;
	}

	if (OwnerASC.IsValid())
	{
		if (UWorld* World = OwnerASC->GetWorld())
		{
			World->GetTimerManager().ClearTimer(UpdateTimerHandle);
		}
	}
	
	UpdateTimerHandle.Invalidate();
	DEBUG_LOG(TEXT("Update timer stopped"));
}