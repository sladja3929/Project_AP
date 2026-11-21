#include "GAS/Abilities/Player/RestAbility.h"
#include "Characters/ItemManagerComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/Abilities/Tasks/AbilityTask_PlayMontageWithEvents.h"
#include "GAS/AbilitySystemComponent/ActionPracticeAbilitySystemComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/InteractionComponent.h"
#include "Characters/WeaponManagerComponent.h"
#include "Items/Weapon.h"
#include "Interaction/Bonfire.h"
#include "Games/ActionPracticePlayerController.h"
#include "Games/ActionPracticeGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Effects/MaterialFadeCameraModifier.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Kismet/GameplayStatics.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogRestAbility, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogRestAbility, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

namespace
{
	//M_PP_RestLightMask의 화면 가림 중심 파라미터
	const FName RestMaskFocusParamName(TEXT("FocusLocation"));
}

URestAbility::URestAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URestAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	AbilityRestTag = UGameplayTagsSubsystem::GetAbilityRestTag();
	StateRestingTag = UGameplayTagsSubsystem::GetStateRestingTag();

	if (!AbilityRestTag.IsValid())
	{
		DEBUG_LOG(TEXT("AbilityRestTag is not valid"));
	}
	if (!StateRestingTag.IsValid())
	{
		DEBUG_LOG(TEXT("StateRestingTag is not valid"));
	}
}

void URestAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CurrentRestState = ERestState::EnteringRest;

	//Bonfire 참조 획득 (1순위: TriggerEventData->OptionalObject, 2순위: InteractionComponent 캐시)
	AcquireBonfire(TriggerEventData);

	//LastActivatedBonfire 갱신
	UpdateLastActivatedBonfire();

	//이동 비활성화
	DisableCharacterMovement();

	//무기 숨김 (어빌리티 전 구간)
	SetWeaponsVisibility(false);

	//Bonfire 방향으로 회전
	//MOVE_None 상태에선 CMC가 루트 모션을 적용하지 않아 워핑 대신 틱 회전 사용 (ServerInitiated라 서버/클라 각자 계산)
	if (CachedBonfire.IsValid())
	{
		if (AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo())
		{
			Character->RotateToTargetPosition(CachedBonfire->GetActorLocation(), RotateToBonfireTime);
		}
	}

	//회전 대기 후 앉기 몽타주 시작
	WaitDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RotateToBonfireTime);
	if (WaitDelayTask)
	{
		WaitDelayTask->OnFinish.AddDynamic(this, &URestAbility::OnRotateDelayFinished);
		WaitDelayTask->ReadyForActivation();
	}
	else
	{
		StartSitMontage();
	}
}

#pragma region "Montage Settings"

UAnimMontage* URestAbility::SetMontageToPlayTask()
{
	switch (CurrentRestState)
	{
	case ERestState::EnteringRest: return SitMontage;
	case ERestState::RestLoop:     return LoopMontage;
	case ERestState::LeavingRest:  return StandMontage;
	}
	return nullptr;
}

void URestAbility::SetUpPlayMontageWithEventsTask()
{
	if (!PlayMontageWithEventsTask)
	{
		DEBUG_LOG(TEXT("SetUpPlayMontageWithEventsTask: No task"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PlayMontageWithEventsTask->OnMontageBlendOut.AddDynamic(this, &URestAbility::OnTaskMontageBlendOut);
	PlayMontageWithEventsTask->OnMontageCompleted.AddDynamic(this, &URestAbility::OnTaskMontageCompleted);
	PlayMontageWithEventsTask->OnMontageInterrupted.AddDynamic(this, &URestAbility::OnTaskMontageInterrupted);
}

void URestAbility::StartMontageWithEventsTask()
{
	UAnimMontage* MontageToPlay = SetMontageToPlayTask();
	if (!MontageToPlay)
	{
		DEBUG_LOG(TEXT("StartMontageWithEventsTask: No montage for state %d"), (int32)CurrentRestState);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	//기존 태스크 정리
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}

	PlayMontageWithEventsTask = UAbilityTask_PlayMontageWithEvents::CreatePlayMontageWithEventsProxy(
		this,
		NAME_None,
		MontageToPlay,
		1.0f,
		NAME_None,
		1.0f
	);

	SetUpPlayMontageWithEventsTask();
	PlayMontageWithEventsTask->ReadyForActivation();
}

void URestAbility::OnTaskMontageBlendOut()
{
	DEBUG_LOG(TEXT("OnTaskMontageBlendOut - State: %d"), (int32)CurrentRestState);

	//Sit → Loop 전환: BlendOut 시점에 Loop 몽타주 시작하여 ABP 상태머신 포즈 노출 방지
	if (CurrentRestState == ERestState::EnteringRest)
	{
		TransitionToLoopMontage();
	}
}

void URestAbility::OnTaskMontageCompleted()
{
	DEBUG_LOG(TEXT("OnTaskMontageCompleted - State: %d"), (int32)CurrentRestState);

	switch (CurrentRestState)
	{
	case ERestState::EnteringRest:
		//BlendOut 없이 즉시 완료된 경우 폴백
		TransitionToLoopMontage();
		break;

	case ERestState::LeavingRest:
		//일어나기 완료 → 이동 복구 후 종료
		RestoreCharacterMovement();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		break;

	default:
		break;
	}
}

void URestAbility::OnTaskMontageInterrupted()
{
	DEBUG_LOG(TEXT("OnTaskMontageInterrupted - State: %d"), (int32)CurrentRestState);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

#pragma endregion

#pragma region "Flow Control"

void URestAbility::StartSitMontage()
{
	CurrentRestState = ERestState::EnteringRest;
	StartMontageWithEventsTask();
}

void URestAbility::TransitionToLoopMontage()
{
	CurrentRestState = ERestState::RestLoop;

	//루프 몽타주 즉시 시작 (Sit BlendOut 후 ABP 포즈 노출 방지)
	StartMontageWithEventsTask();

	if (!IsActive()) return;

	//화면을 덮은 뒤 회복·적 리셋 (서버/클라 인스턴스가 각자 같은 타임라인으로 진행, 화면 가림은 로컬만)
	SetLocalRestScreenMask(true, RestFadeOutTime);

	END_ABILITY_TASK(RestFadeDelayTask);
	RestFadeDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RestFadeOutTime);
	if (RestFadeDelayTask)
	{
		RestFadeDelayTask->OnFinish.AddDynamic(this, &URestAbility::OnRestFadeOutFinished);
		RestFadeDelayTask->ReadyForActivation();
	}
	else
	{
		OnRestFadeOutFinished();
	}
}

void URestAbility::OnRestFadeOutFinished()
{
	//회복·적 리셋은 서버만 실행, 클라 인스턴스는 페이드 타이머만 진행 (결과는 복제로 수신)
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		ApplyRestRecovery();
		RequestEnemyReset();
	}

	//리스폰 복제가 도착할 때까지 덮인 상태 유지
	END_ABILITY_TASK(RestFadeDelayTask);
	RestFadeDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RestFadeHoldTime);
	if (RestFadeDelayTask)
	{
		RestFadeDelayTask->OnFinish.AddDynamic(this, &URestAbility::OnRestFadeHoldFinished);
		RestFadeDelayTask->ReadyForActivation();
	}
	else
	{
		OnRestFadeHoldFinished();
	}
}

void URestAbility::OnRestFadeHoldFinished()
{
	END_ABILITY_TASK(RestFadeDelayTask);

	//화면 가림은 앉아 있는 동안 유지, 일어설 때 해제
	//재입력 대기: 리셋 후 최소 유지 시간이 지나야 허용, PlayerController가 Ability.Rest 이벤트를 보내면 종료
	START_WAIT_EVENT_TASK(WaitExitRestEventTask, AbilityRestTag, OnExitRestEventReceived, nullptr, true, true);
	DEBUG_LOG(TEXT("OnRestFadeHoldFinished: Waiting for exit event (Ability.Rest)"));
}

void URestAbility::StartStandMontage()
{
	CurrentRestState = ERestState::LeavingRest;

	//루프 몽타주 중단
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}

	StartMontageWithEventsTask();
}

#pragma endregion

#pragma region "Helper"

void URestAbility::SetWeaponsVisibility(bool bVisible)
{
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	UWeaponManagerComponent* WeaponManager = Character->GetWeaponManagerComponent();
	if (!WeaponManager) return;

	if (AWeapon* LeftWeapon = WeaponManager->GetLeftWeapon())
	{
		LeftWeapon->SetActorHiddenInGame(!bVisible);
	}

	if (AWeapon* RightWeapon = WeaponManager->GetRightWeapon())
	{
		RightWeapon->SetActorHiddenInGame(!bVisible);
	}

	DEBUG_LOG(TEXT("SetWeaponsVisibility: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void URestAbility::AcquireBonfire(const FGameplayEventData* TriggerEventData)
{
	//1순위: TriggerEventData->OptionalObject (Bonfire::Interact()의 HandleGameplayEvent 경로)
	if (TriggerEventData && TriggerEventData->OptionalObject)
	{
		//OptionalObject는 const UObject*이므로 const_cast 후 캐스팅
		CachedBonfire = Cast<ABonfire>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));
		if (CachedBonfire.IsValid())
		{
			DEBUG_LOG(TEXT("AcquireBonfire: From TriggerEventData.OptionalObject"));
			return;
		}
	}

	//2순위: InteractionComponent 캐시
	if (AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo())
	{
		if (UInteractionComponent* InteractionComp = Character->GetInteractionComponent())
		{
			CachedBonfire = Cast<ABonfire>(InteractionComp->GetCurrentInteractable());
			if (CachedBonfire.IsValid())
			{
				DEBUG_LOG(TEXT("AcquireBonfire: From InteractionComponent cache"));
				return;
			}
		}
	}

	DEBUG_LOG(TEXT("AcquireBonfire: Bonfire reference not found"));
}

void URestAbility::UpdateLastActivatedBonfire()
{
	if (!CachedBonfire.IsValid()) return;

	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (!Character) return;

	AActionPracticePlayerController* PC = Cast<AActionPracticePlayerController>(Character->GetController());
	if (!PC) return;

	PC->SetLastActivatedBonfire(CachedBonfire.Get());
	DEBUG_LOG(TEXT("UpdateLastActivatedBonfire: %s"), *GetNameSafe(CachedBonfire.Get()));
}

void URestAbility::DisableCharacterMovement()
{
	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character) return;

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp) return;

	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
	DEBUG_LOG(TEXT("DisableCharacterMovement"));
}

void URestAbility::RestoreCharacterMovement()
{
	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character) return;

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp) return;

	if (MoveComp->MovementMode == MOVE_None)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		DEBUG_LOG(TEXT("RestoreCharacterMovement: Walking mode restored"));
	}
}

void URestAbility::ApplyRestRecovery()
{
	if (!RestRecoveryEffect)
	{
		DEBUG_LOG(TEXT("ApplyRestRecovery: RestRecoveryEffectClass not set"));
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	const float Level = static_cast<float>(GetAbilityLevel());
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(RestRecoveryEffect, Level, ASC->MakeEffectContext());
	if (!Spec.IsValid()) return;

	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	DEBUG_LOG(TEXT("ApplyRestRecovery: GE applied"));

	//Refillable 아이템 리필
	AActionPracticeCharacter* Character = GetActionPracticeCharacterFromActorInfo();
	if (Character)
	{
		UItemManagerComponent* ItemManager = Character->GetItemManagerComponent();
		if (ItemManager)
		{
			ItemManager->RefillAllSlots();
			DEBUG_LOG(TEXT("ApplyRestRecovery: Refillable items refilled"));
		}
	}
}

void URestAbility::RequestEnemyReset()
{
	UWorld* World = GetWorld();
	if (!World) return;

	AActionPracticeGameMode* GameMode = Cast<AActionPracticeGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	GameMode->ResetAllEnemies();
	DEBUG_LOG(TEXT("RequestEnemyReset: ResetAllEnemies called"));
}

APlayerCameraManager* URestAbility::GetLocalCameraManager() const
{
	//화면 연출이므로 로컬 플레이어만 (데디 서버·원격 클라 인스턴스 제외)
	if (!IsLocallyControlled()) return nullptr;

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	return PC ? PC->PlayerCameraManager.Get() : nullptr;
}

void URestAbility::SetLocalRestScreenMask(bool bCover, float Duration)
{
	APlayerCameraManager* CameraManager = GetLocalCameraManager();
	if (!CameraManager) return;

	//모디파이어는 PCM에 한 번 추가 후 재사용 (해제 시 Alpha 0에서 스스로 비활성화)
	UMaterialFadeCameraModifier* Modifier = Cast<UMaterialFadeCameraModifier>(
		CameraManager->FindCameraModifierByClass(UMaterialFadeCameraModifier::StaticClass()));

	if (bCover)
	{
		if (!RestScreenMaskMaterial)
		{
			DEBUG_LOG(TEXT("SetLocalRestScreenMask: RestScreenMaskMaterial not set"));
			return;
		}

		if (!Modifier)
		{
			Modifier = Cast<UMaterialFadeCameraModifier>(CameraManager->AddNewCameraModifier(UMaterialFadeCameraModifier::StaticClass()));
			if (!Modifier) return;
		}

		//모닥불 주변만 남기고 어둡게 덮음 (모닥불 참조가 없으면 캐릭터 기준)
		const AActor* FocusActor = CachedBonfire.IsValid() ? static_cast<const AActor*>(CachedBonfire.Get()) : GetAvatarActorFromActorInfo();
		const FVector FocusLocation = FocusActor ? FocusActor->GetActorLocation() : FVector::ZeroVector;

		Modifier->SetFadeMaterial(RestScreenMaskMaterial);
		Modifier->SetVectorParameter(RestMaskFocusParamName, FLinearColor(FocusLocation));
		Modifier->BlendIn(Duration);
	}
	else if (Modifier)
	{
		Modifier->BlendOut(Duration);
	}

	bRestFadeCovering = bCover;
	DEBUG_LOG(TEXT("SetLocalRestScreenMask: %s (%.2fs)"), bCover ? TEXT("Cover") : TEXT("Uncover"), Duration);
}

void URestAbility::OnRotateDelayFinished()
{
	StartSitMontage();
}

void URestAbility::OnExitRestEventReceived(FGameplayEventData Payload)
{
	DEBUG_LOG(TEXT("OnExitRestEventReceived: Starting stand montage"));

	//일어서기와 함께 화면 가림 해제
	SetLocalRestScreenMask(false, RestFadeInTime);
	StartStandMontage();
}

#pragma endregion

void URestAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	//취소/인터럽트 포함 이동 복구 및 무기 복구 보장
	RestoreCharacterMovement();
	SetWeaponsVisibility(true);

	//화면 가림 도중 취소된 경우 현재 강도에서 화면 복구
	if (bRestFadeCovering)
	{
		SetLocalRestScreenMask(false, RestFadeInTime);
		bRestFadeCovering = false;
	}

	//태스크 정리
	if (PlayMontageWithEventsTask)
	{
		PlayMontageWithEventsTask->StopMontage();
		PlayMontageWithEventsTask->EndTask();
		PlayMontageWithEventsTask = nullptr;
	}
	END_ABILITY_TASK(WaitDelayTask);
	END_ABILITY_TASK(RestFadeDelayTask);
	END_ABILITY_TASK(WaitExitRestEventTask);

	CachedBonfire = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
