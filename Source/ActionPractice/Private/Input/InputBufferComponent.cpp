#include "Input/InputBufferComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Input/InputActionDataAsset.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogInputBufferComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogInputBufferComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif


UInputBufferComponent::UInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	//컴포넌트 복제 활성화
	SetIsReplicatedByDefault(true);
}

void UInputBufferComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//COND_OwnerOnly로 소유 클라만 받도록 (트래픽 최소화)
	DOREPLIFETIME_CONDITION(UInputBufferComponent, bServerBufferEnabled, COND_OwnerOnly);
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

	//이벤트 바인딩
	UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent();
	if (ASC)
	{
		EnableBufferInputHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(EventNotifyEnableBufferInputTag).AddUObject(this, &UInputBufferComponent::OnEventEnableBufferInput);
		PlayBufferHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(EventActionPlayBufferTag).AddUObject(this, &UInputBufferComponent::OnEventPlayBuffer);
	}

	Super::BeginPlay();
}

#pragma region "Buffer Action Functions"
bool UInputBufferComponent::CheckActionRule(FGameplayTag ActionTag, int32& OutPriority, bool& bIsHoldAction) const
{
	if (!ActionTag.IsValid() || !OwnerCharacter || !CachedInputActionData)
	{
		DEBUG_LOG(TEXT("CheckActionRule: No Character or Data Asset"));
		bIsHoldAction = false;
		return false;
	}

	const UInputAction* InputAction = CachedInputActionData->FindInputActionByTag(ActionTag);
	if (!InputAction)
	{
		DEBUG_LOG(TEXT("CheckActionRule: No IA - %s"), *ActionTag.ToString());
		OutPriority = -1;
		bIsHoldAction = false;
		return false;
	}
	
	const FInputActionAbilityRule* InputActionRule = CachedInputActionData->FindRuleByAction(InputAction);
	if (!InputActionRule)
	{
		DEBUG_LOG(TEXT("CheckActionRule: No Rule - %s"), *ActionTag.ToString());
		OutPriority = -1;
		bIsHoldAction = false;
		return false;
	}

	OutPriority = InputActionRule->BufferPriority;
	bIsHoldAction = InputActionRule->bIsHoldAction;
	return InputActionRule->bCanBuffered;
}

void UInputBufferComponent::BufferInput(const UInputAction* InputAction, bool bIsReleased)
{
	if (!InputAction || !OwnerCharacter || !CachedInputActionData)
	{
		return;
	}

	//버퍼 윈도우가 닫혀있으면 아무것도 하지 않음
	if (!bInternalBufferEnabled)
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

	//release 입력은 단발 액션이면 “실행 플래그” 의미, hold 액션이면 “해제(버퍼 제거)” 의미가 될 수 있으므로
	//태그/룰 판별은 아래 내부 로직(BufferInputInternal/ServerBufferInput)에서 처리되게 둠.

	if (OwnerActor->HasAuthority())
	{
		//Standalone/Server: 공통 버퍼 상태에 직접 반영
		InternalBufferInput(InputTag, bIsReleased);
	}
	
	else
	{
		//Client: 서버에 태그 기반 입력 전달 (TTL/그레이스 포함)
		ServerBufferInput(InputTag, bIsReleased);
	}
}

void UInputBufferComponent::InternalBufferInput(FGameplayTag InputActionTag, bool bIsReleased)
{
	int32 NewActionPriority = -1;
	bool bIsHoldAction = false;

	if (!CheckActionRule(InputActionTag, NewActionPriority, bIsHoldAction))
	{
		DEBUG_LOG(TEXT("BufferInputInternal Cannot buffer - %s"), *InputActionTag.ToString());
		return;
	}

	//단발 액션 Released는 무시
	if (bIsReleased && !bIsHoldAction)
	{
		DEBUG_LOG(TEXT("BufferInputInternal Ignored - Non-hold released %s"), *InputActionTag.ToString());
		return;
	}

	//홀드 액션일 경우
	if (bIsHoldAction)
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
	}

	//단발 액션일 경우
	else
	{
		if (NewActionPriority >= BufferPriority)
		{
			BufferedActionTag = InputActionTag;
			BufferPriority = NewActionPriority;
			bBufferedActionReleased = bIsReleased;

			DEBUG_LOG(TEXT("BufferInputInternal Action buffered - %s Priority %d Released %s"),
					  *InputActionTag.ToString(),
					  NewActionPriority,
					  bIsReleased ? TEXT("true") : TEXT("false"));
		}
		
		else
		{
			DEBUG_LOG(TEXT("BufferInputInternal Action ignored - Lower priority %d vs %d"),
					  NewActionPriority, BufferPriority);
		}
	}
}

bool UInputBufferComponent::IsBufferWaiting()
{
	return BufferedActionTag.IsValid() || (BufferedHoldActionTags.Num() > 0);
}

void UInputBufferComponent::ExecuteBuffer()
{
	if (!OwnerCharacter)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	//클라이언트는 아예 저장된 버퍼 실행에 관여하지 못함
	if (!OwnerActor->HasAuthority()) 
	{
		return;
	}

	//윈도우 닫기
	bInternalBufferEnabled = false;
	bServerBufferEnabled = false;
	
	DEBUG_LOG(TEXT("ExecuteBuffer called"));

	//저장된 단발 액션 실행
	if (BufferedActionTag.IsValid())
	{
		ActivateAbilityByTag(BufferedActionTag);

		BufferedActionTag = FGameplayTag();
		BufferPriority = -1;
		bBufferedActionReleased = false;

		DEBUG_LOG(TEXT("ExecuteBuffer: Normal buffer executed"));
	}

	//저장된 모든 홀드 액션 실행
	else if (BufferedHoldActionTags.Num() > 0)
	{
		for (const FGameplayTag& Tag : BufferedHoldActionTags)
		{
			ActivateAbilityByTag(Tag);
		}

		BufferedHoldActionTags.Empty();
		DEBUG_LOG(TEXT("ExecuteBuffer: Hold buffers executed"));
	}
	
	else
	{
		DEBUG_LOG(TEXT("ExecuteBuffer: No buffered action"));
	}
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
	if (!InputAction || !OwnerCharacter) return;
	
	UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent();
	if (!ASC) return;

	TArray<FGameplayAbilitySpec*> TryActivateSpecs = OwnerCharacter->FindAbilitySpecsWithInputAction(InputAction);
	if (TryActivateSpecs.IsEmpty()) return;

	for (auto& Spec : TryActivateSpecs)
	{
		DEBUG_LOG(TEXT("CurrSpec: %s"), *Spec->Handle.ToString());
		//첫 실행이거나, bRetriggerInstancedAbility = true여서 재실행될 때
		if (ASC->TryActivateAbility(Spec->Handle))
		{
			DEBUG_LOG(TEXT("Play Buffer - Activate Ability: %s"), *GetNameSafe(Spec->Ability->GetClass()));
			Spec->InputPressed = true;
		}

		//어빌리티가 이미 실행중 / bRetriggerInstancedAbility = false여서 Try를 실패했을 때 (콤보 공격)
		else if (Spec->IsActive()) //CanActivateAbility 실패로 활성화하지 못했을 때를 거르기 위해 실행 중 체크
		{
			DEBUG_LOG(TEXT("Play Buffer - Play Buffer Event: %s"), *GetNameSafe(Spec->Ability->GetClass()));
			Spec->InputPressed = true;

			//현재 Spec인 어빌리티만 OnInputByBuffer가 활성화되도록 자기 자신을 EventData로 넘김
			UGameplayAbility* Instance = Spec->GetPrimaryInstance();
			if (!Instance) Instance = Spec->Ability;
			
			FGameplayEventData EventData;
			EventData.OptionalObject = Instance;
			//bool 값을 EventMagnitude를 통해 전달
			EventData.EventMagnitude = bBufferActionReleased ? 1.0f : 0.0f;
			EventData.EventTag = EventActionInputByBufferTag;
			
			ASC->HandleGameplayEvent(EventActionInputByBufferTag, &EventData);
		}

		//다 아닐때
		else DEBUG_LOG(TEXT("Play Buffer Activate Failed: %s"), *GetNameSafe(Spec->Ability->GetClass()));
	}
}

void UInputBufferComponent::EnableBufferInput(bool bEnabled)
{
	//클라이언트 버퍼 윈도우는 항상 변경
	bInternalBufferEnabled = bEnabled;

	if (!OwnerCharacter)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	//서버/스탠드어론: 서버 버퍼 윈도우 변경
	if (OwnerActor->HasAuthority())
	{
		bServerBufferEnabled = bEnabled;

		//서버가 윈도우 오픈할 때 지연된 입력 버퍼에 저장
		if (bEnabled)
		{
			ProcessPendingInputs();
		}
	}

	DEBUG_LOG(TEXT("EnableBufferInput %s"), bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
}

#pragma endregion

#pragma region "Server RPC Functions"

void UInputBufferComponent::ServerBufferInput_Implementation(FGameplayTag InputActionTag, bool bIsReleased)
{
	if (!CachedInputActionData)
	{
		DEBUG_LOG(TEXT("ServerBufferInput CachedInputActionData is null"));
		return;
	}

	int32 Priority = -1;
	bool bIsHoldAction = false;
	if (!CheckActionRule(InputActionTag, Priority, bIsHoldAction))
	{
		DEBUG_LOG(TEXT("ServerBufferInput Cannot buffer action for tag %s"), *InputActionTag.ToString());
		return;
	}

	//홀드 해제는 즉시 제거
	if (bIsReleased && bIsHoldAction)
	{
		BufferedHoldActionTags.Remove(InputActionTag);
		PendingInputs.Remove(InputActionTag);
		DEBUG_LOG(TEXT("ServerBufferInput Hold action released - %s"), *InputActionTag.ToString());
		return;
	}

	//서버 윈도우가 열려있으면 바로 저장 가능
	if (bServerBufferEnabled)
	{
		InternalBufferInput(InputActionTag, bIsReleased);
	}

	//서버 윈도우가 닫혀있으면 네트워크 지연을 고려해서 TTL 저장
	else
	{
		FPendingInput Pending;
		Pending.ActionTag    = InputActionTag;
		Pending.bIsReleased  = bIsReleased;
		Pending.Timestamp    = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		Pending.bIsHoldAction = bIsHoldAction;

		PendingInputs.Add(InputActionTag, Pending);

		DEBUG_LOG(TEXT("ServerBufferInput Input pending - %s Buffer disabled"),
				  *InputActionTag.ToString());

		CleanupExpiredPendingInputs();
	}
}

void UInputBufferComponent::ProcessPendingInputs()
{
	bool ba = true;
	if (ba) return;
	if (!GetWorld())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	for (const auto& Pair : PendingInputs)
	{
		const FPendingInput& Pending = Pair.Value;

		if (CurrentTime - Pending.Timestamp <= InputGracePeriod)
		{
			InternalBufferInput(Pending.ActionTag, Pending.bIsReleased);
			DEBUG_LOG(TEXT("ProcessPendingInputs Processed - %s"), *Pending.ActionTag.ToString());
		}
		else
		{
			DEBUG_LOG(TEXT("ProcessPendingInputs Expired - %s"), *Pending.ActionTag.ToString());
		}
	}

	PendingInputs.Empty();
}

void UInputBufferComponent::CleanupExpiredPendingInputs()
{
	if (!GetWorld())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	for (auto It = PendingInputs.CreateIterator(); It; ++It)
	{
		if (CurrentTime - It.Value().Timestamp > InputGracePeriod)
		{
			DEBUG_LOG(TEXT("CleanupExpiredPendingInputs Removed expired - %s"),
					  *It.Key().ToString());
			It.RemoveCurrent();
		}
	}
}

void UInputBufferComponent::OnRepBufferState()
{
	//bInternalBufferEnabled = bServerBufferEnabled;
	
	//DEBUG_LOG(TEXT("OnRepBufferState %s"), bServerBufferEnabled ? TEXT("Enabled") : TEXT("Disabled"));
}

#pragma endregion

void UInputBufferComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//이벤트 해제
	if (OwnerCharacter)
	{
		UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent();
		if (ASC)
		{
			if (EnableBufferInputHandle.IsValid())
			{
				ASC->GenericGameplayEventCallbacks.FindOrAdd(EventNotifyEnableBufferInputTag).Remove(EnableBufferInputHandle);
				EnableBufferInputHandle.Reset();
			}
			if (PlayBufferHandle.IsValid())
			{
				ASC->GenericGameplayEventCallbacks.FindOrAdd(EventActionPlayBufferTag).Remove(PlayBufferHandle);
				PlayBufferHandle.Reset();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UInputBufferComponent::OnEventEnableBufferInput(const FGameplayEventData* Payload)
{
	if (!Payload)
	{
		return;
	}

	//EventMagnitude가 1이면 Enable, 0이면 Disable
	const bool bEnabled = Payload->EventMagnitude > 0.5f;
	EnableBufferInput(bEnabled);
	DEBUG_LOG(TEXT("OnEventEnableBufferInput: %s"), bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
}

void UInputBufferComponent::OnEventPlayBuffer(const FGameplayEventData* Payload)
{
	ExecuteBuffer();
	DEBUG_LOG(TEXT("OnEventPlayBuffer: Buffer Executed"));
}
