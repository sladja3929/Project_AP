#include "Input/InputBufferComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "Input/InputActionDataAsset.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogInputBufferComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogInputBufferComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif


UInputBufferComponent::UInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInputBufferComponent::BeginPlay()
{
	AActor* CurrentOwner = GetOwner();
	OwnerCharacter = Cast<AActionPracticeCharacter>(CurrentOwner);

	if (!OwnerCharacter)
	{
		return;
	}

	//InputActionData 캐싱
	CachedInputActionData = OwnerCharacter->GetInputActionData();

	//태그 초기화
	EventNotifyEnableBufferInputTag = UGameplayTagsSubsystem::GetEventNotifyEnableBufferInputTag();
	EventActionInputByBufferTag = UGameplayTagsSubsystem::GetEventActionInputByBufferTag();
	EventActionPlayBufferTag = UGameplayTagsSubsystem::GetEventActionPlayBufferTag();

	if (!EventNotifyEnableBufferInputTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventNotifyEnableBufferInputTag is not valid"));
	}
	if (!EventActionInputByBufferTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventActionInputByBufferTag is not valid"));
	}
	if (!EventActionPlayBufferTag.IsValid())
	{
		DEBUG_LOG(TEXT("EventActionPlayBufferTag is not valid"));
	}

	Super::BeginPlay();
}

#pragma region "Buffer Action Functions"
bool UInputBufferComponent::CheckActionRule(FGameplayTag ActionTag, int32& OutPriority, EInputBehavior& OutInputBehavior) const
{
	if (!ActionTag.IsValid() || !OwnerCharacter || !CachedInputActionData)
	{
		DEBUG_LOG(TEXT("CheckActionRule: No Character or Data Asset"));
		OutInputBehavior = EInputBehavior::Tap;
		return false;
	}

	const UInputAction* InputAction = CachedInputActionData->FindInputActionByTag(ActionTag);
	if (!InputAction)
	{
		DEBUG_LOG(TEXT("CheckActionRule: No IA - %s"), *ActionTag.ToString());
		OutPriority = -1;
		OutInputBehavior = EInputBehavior::Tap;
		return false;
	}

	const FInputActionAbilityRule* InputActionRule = CachedInputActionData->FindRuleByAction(InputAction);
	if (!InputActionRule)
	{
		DEBUG_LOG(TEXT("CheckActionRule: No Rule - %s"), *ActionTag.ToString());
		OutPriority = -1;
		OutInputBehavior = EInputBehavior::Tap;
		return false;
	}

	OutPriority = InputActionRule->BufferPriority;
	OutInputBehavior = InputActionRule->InputBehavior;
	return InputActionRule->bCanBuffered;
}

void UInputBufferComponent::BufferInput(const UInputAction* InputAction, bool bIsReleased)
{
	if (!InputAction || !OwnerCharacter || !CachedInputActionData)
	{
		return;
	}

	//버퍼 윈도우가 닫혀있으면 아무것도 하지 않음
	if (!bBufferWindowOpened)
	{
		return;
	}

	const FGameplayTag InputTag = CachedInputActionData->FindTagByInputAction(InputAction);
	if (!InputTag.IsValid())
	{
		DEBUG_LOG(TEXT("BufferInput - No tag for action %s"), *GetNameSafe(InputAction));
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	//소유 클라이언트 확인
	if (!OwnerCharacter->IsLocallyControlled())
	{
		return;
	}
	
	BufferInputInternal(InputTag, bIsReleased);
}

void UInputBufferComponent::BufferInputInternal(FGameplayTag InputActionTag, bool bIsReleased)
{
	int32 NewActionPriority = -1;
	EInputBehavior InputBehavior = EInputBehavior::Tap;

	if (!CheckActionRule(InputActionTag, NewActionPriority, InputBehavior))
	{
		DEBUG_LOG(TEXT("BufferInputInternal Cannot buffer - %s"), *InputActionTag.ToString());
		return;
	}

	//Tap 액션 Released는 무시
	if (bIsReleased && InputBehavior == EInputBehavior::Tap)
	{
		DEBUG_LOG(TEXT("BufferInputInternal Ignored - Tap released %s"), *InputActionTag.ToString());
		return;
	}

	//Hold 액션일 경우 목록에 추가/제거
	if (InputBehavior == EInputBehavior::Hold)
	{
		if (bIsReleased)
		{
			BufferedHoldActionTags.Remove(InputActionTag);
			DEBUG_LOG(TEXT("BufferInputInternal Hold action removed - %s"), *InputActionTag.ToString());
		}

		else
		{
			BufferedHoldActionTags.Add(InputActionTag);
			DEBUG_LOG(TEXT("BufferInputInternal Hold action added - %s"), *InputActionTag.ToString());
		}
		return;
	}

	//Tap 또는 Both 액션 Pressed일 경우 우선순위 비교 후 저장
	if (!bIsReleased)
	{
		if (NewActionPriority < BufferPriority)
		{
			DEBUG_LOG(TEXT("BufferInputInternal Action ignored - Lower priority %d vs %d"), NewActionPriority, BufferPriority);
			return;
		}

		BufferedActionTag = InputActionTag;
		BufferPriority = NewActionPriority;

		//버퍼 저장 시점의 상태 태그 캡처
		CaptureCurrentStateTags();

		DEBUG_LOG(TEXT("BufferInputInternal Action buffered - %s Priority: %d"), *InputActionTag.ToString(), NewActionPriority);
		return;
	}	
	
	//현재 저장된 Both 액션의 Released일 경우 단발로 확정 (플래그 설정)
	if (BufferedActionTag == InputActionTag) 
	{
		bBufferedActionReleased = true;
		DEBUG_LOG(TEXT("BufferInputInternal Buffered action released - %s"), *InputActionTag.ToString());
	}
}

bool UInputBufferComponent::IsBufferWaiting()
{
	return BufferedActionTag.IsValid() || (BufferedHoldActionTags.Num() > 0);
}

void UInputBufferComponent::ExecuteBuffer()
{
	if (!OwnerCharacter) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	//프록시는 저장된 버퍼 실행에 관여하지 못함
	if (!OwnerCharacter->IsLocallyControlled()) return;

	//윈도우 닫기
	bBufferWindowOpened = false;

	DEBUG_LOG(TEXT("ExecuteBuffer called"));

	//저장된 단발 액션 실행 (dispatch 전에 먼저 소비)
	if (BufferedActionTag.IsValid())
	{
		//★ consume-before-dispatch: dispatch 중 재귀가 들어와도 빈 버퍼를 보도록 멤버를 먼저 클리어
		const FGameplayTag TagToActivate = BufferedActionTag;
		BufferedActionTag = FGameplayTag();
		BufferPriority = -1;
		bBufferedActionReleased = false;

		ActivateAbilityByTag(TagToActivate);
		DEBUG_LOG(TEXT("ExecuteBuffer: Normal buffer executed"));
		return;
	}

	//저장된 모든 홀드 액션 실행 (dispatch 전에 먼저 소비)
	if (BufferedHoldActionTags.Num() > 0)
	{
		//★ consume-before-dispatch: 소유권을 로컬로 옮기고 멤버는 즉시 비움
		TSet<FGameplayTag> HoldToActivate = MoveTemp(BufferedHoldActionTags);
		BufferedHoldActionTags.Reset();

		for (const FGameplayTag& Tag : HoldToActivate)
		{
			ActivateAbilityByTag(Tag);
		}
		DEBUG_LOG(TEXT("ExecuteBuffer: Hold buffers executed"));
		return;
	}

	DEBUG_LOG(TEXT("ExecuteBuffer: No buffered action"));
}

void UInputBufferComponent::ActivateAbilityByTag(FGameplayTag ActionTag)
{
	if (!ActionTag.IsValid() || !OwnerCharacter || !CachedInputActionData)
	{
		return;
	}

	const UInputAction* InputAction = CachedInputActionData->FindInputActionByTag(ActionTag);
	if (!InputAction)
	{
		DEBUG_LOG(TEXT("ActivateAbilityByTag InputAction not found for tag %s"), *ActionTag.ToString());
		return;
	}

	ActivateAbility(InputAction);
}

void UInputBufferComponent::ActivateAbility(const UInputAction* InputAction)
{
	if (!InputAction || !OwnerCharacter || !CachedInputActionData) return;
	
	UActionPracticeAbilitySystemComponent* APASC = Cast<UActionPracticeAbilitySystemComponent>(OwnerCharacter->GetAbilitySystemComponent());
	if (!APASC) return;

	TArray<FGameplayAbilitySpec*> TryActivateSpecs = OwnerCharacter->FindAbilitySpecsWithInputAction(InputAction);
	if (TryActivateSpecs.IsEmpty()) return;
	
	for (auto& Spec : TryActivateSpecs)
	{
		DEBUG_LOG(TEXT("CurrSpecNum: %s"), *Spec->Handle.ToString());

		//어빌리티가 이미 실행중 or bRetriggerInstancedAbility = false여서 Try를 실패했을 때 (콤보 공격)
		if (Spec->IsActive()) //CanActivateAbility 실패로 활성화하지 못했을 때를 거르기 위해 실행 중 체크
		{
			DEBUG_LOG(TEXT("Play Buffer - Play Buffer Event: %s"), *GetNameSafe(Spec->Ability->GetClass()));
			Spec->InputPressed = true;

			FGameplayEventData EventData;
			EventData.InstigatorTags.AddTag(CachedInputActionData->FindTagByInputAction(InputAction)); //타깃 어빌리티만 활성화

			//버퍼 저장 시점의 상태 태그 추가
			EventData.InstigatorTags.AppendTags(BufferedStateTags);

			EventData.EventMagnitude = bBufferedActionReleased ? 1.0f : 0.0f; //release 여부를 EventMagnitude를 통해 전달
			EventData.EventTag = EventActionInputByBufferTag;

			APASC->HandleGameplayEvent_NetPredicted(EventActionInputByBufferTag, &EventData);
		}
		
		//첫 실행 or bRetriggerInstancedAbility = true여서 재실행될 때
		else
		{
			//BufferedActionTag를 EventData에 담아 전달
			FGameplayEventData ActivateEventData;
			FGameplayTag InputTag = CachedInputActionData->FindTagByInputAction(InputAction);
			ActivateEventData.InstigatorTags.AddTag(InputTag);

			//TryActivate 내부에서 ActivateAbility가 동기 실행되므로,
			//StartWaitInputReleaseTask(true)가 Spec->InputPressed=false인 채로 실행되면 즉시 종료됨
			//→ 먼저 true로 세팅하여 WaitInputRelease 즉시 발동 방지
			Spec->InputPressed = true;

			if (APASC->TryActivateAbilityWithEventData(Spec->Handle, &ActivateEventData))
			{
				DEBUG_LOG(TEXT("Play Buffer - Activate Ability: %s"), *GetNameSafe(Spec->Ability->GetClass()));
			}
			else
			{
				Spec->InputPressed = false; //활성화 실패 시 원복
				DEBUG_LOG(TEXT("Play Buffer Activate Failed: %s"), *GetNameSafe(Spec->Ability->GetClass()));
			}
		}
	}
}

void UInputBufferComponent::CaptureCurrentStateTags()
{
	BufferedStateTags.Reset();

	if (!OwnerCharacter)
	{
		return;
	}

	UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	//현재 ASC의 모든 태그 가져오기
	FGameplayTagContainer CurrentTags;
	ASC->GetOwnedGameplayTags(CurrentTags);

	//State 태그만 필터링하여 저장
	static const FGameplayTag StateParentTag = FGameplayTag::RequestGameplayTag(FName("State"));
	for (const FGameplayTag& Tag : CurrentTags)
	{
		if (Tag.MatchesTag(StateParentTag))
		{
			BufferedStateTags.AddTag(Tag);
		}
	}

	DEBUG_LOG(TEXT("CaptureCurrentStateTags: %s"), *BufferedStateTags.ToString());
}

void UInputBufferComponent::EnableBufferInput(bool bEnabled)
{
	//클라이언트 버퍼 윈도우는 항상 변경
	bBufferWindowOpened = bEnabled;

	DEBUG_LOG(TEXT("EnableBufferInput %s"), bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
}

#pragma endregion

void UInputBufferComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
