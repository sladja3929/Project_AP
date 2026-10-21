#include "GAS/Abilities/Enemy/EnemyLungeAbility.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "AI/EnemyAIController.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Curves/CurveVector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyLungeAbility, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyLungeAbility, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UEnemyLungeAbility::UEnemyLungeAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

#pragma region "Activate Initialization"

void UEnemyLungeAbility::ActivateInitSettings()
{
	Super::ActivateInitSettings();

	CacheLungeConfig();
	CacheLungeTags();
}

void UEnemyLungeAbility::CacheLungeConfig()
{
	if (EnemyAttackData && EnemyAttackData->AttackType == EComboAttackType::Lunge)
	{
		CachedLungeConfig = &EnemyAttackData->LungeConfig;
		DEBUG_LOG(TEXT("CacheLungeConfig: HeightCurve=%s"),
			CachedLungeConfig->HeightCurve ? TEXT("Set") : TEXT("None"));
	}
	else
	{
		CachedLungeConfig = nullptr;
		DEBUG_LOG(TEXT("CacheLungeConfig: AttackType is not Lunge"));
	}
}

void UEnemyLungeAbility::CacheLungeTags()
{
	EventNotifyTrackingTargetTag = UGameplayTagsSubsystem::GetEventNotifyTrackingTargetTag();
	EventNotifyLungeStartTag = UGameplayTagsSubsystem::GetEventNotifyLungeStartTag();
	EventNotifyLungeEndTag = UGameplayTagsSubsystem::GetEventNotifyLungeEndTag();
}

#pragma endregion

#pragma region "Ability Lifecycle"

void UEnemyLungeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//부모의 ActivateAbility → ActivateInitSettings → ExecuteAttack 호출
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CachedLungeConfig)
	{
		DEBUG_LOG(TEXT("ActivateAbility: No CachedLungeConfig — ending ability"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//초기 타겟 위치 캐싱 (TrackingTarget ANS 시작 전 fallback)
	if (CachedTargetInfo.IsValid() && CachedTargetInfo.Actor.IsValid())
	{
		CachedDestination = CachedTargetInfo.Actor->GetActorLocation();
	}

	//ANS 이벤트 대기 태스크 등록
	START_WAIT_EVENT_TASK(WaitTrackingTargetEventTask, EventNotifyTrackingTargetTag, OnEventTrackingTarget, nullptr, false, true);
	START_WAIT_EVENT_TASK(WaitLungeStartEventTask, EventNotifyLungeStartTag, OnEventLungeStart, nullptr, true, true);
	START_WAIT_EVENT_TASK(WaitLungeEndEventTask, EventNotifyLungeEndTag, OnEventLungeEnd, nullptr, true, true);

	DEBUG_LOG(TEXT("ActivateAbility: Lunge started — MaxCombo=%d"), MaxComboCount);
}

void UEnemyLungeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//RootMotionSource 정리 (어빌리티 중도 취소 대비)
	StopLungeMovement();

	//이벤트 태스크 정리
	END_ABILITY_TASK(WaitTrackingTargetEventTask);
	END_ABILITY_TASK(WaitLungeStartEventTask);
	END_ABILITY_TASK(WaitLungeEndEventTask);

	//상태 초기화
	CachedLungeConfig = nullptr;
	CachedDestination = FVector::ZeroVector;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#pragma endregion

#pragma region "Event Handlers"

void UEnemyLungeAbility::OnEventTrackingTarget(FGameplayEventData Payload)
{
	if (Payload.Target)
	{
		CachedDestination = Payload.Target->GetActorLocation();
	}

	//이동 중이 아니면 위치 캐싱만
	if (!IsLungeActive()) return;

	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy) return;
	UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement();
	if (!CMC) return;

	//CMC에 등록된 소스 인스턴스 조회. 없으면 Duration 만료로 정리된 상태
	const TSharedPtr<FRootMotionSource> Source = CMC->GetRootMotionSourceByID(CurrentLungeSourceID);
	if (!Source.IsValid() || Source->GetScriptStruct() != FRootMotionSource_MoveToDynamicForce::StaticStruct())
	{
		DEBUG_LOG(TEXT("OnEventTrackingTarget: Lunge source not found — stop movement"));
		StopLungeMovement();
		return;
	}

	const float Duration = Source->GetDuration();
	const float Elapsed = Source->GetTime();

	//거의 끝나면 재타게팅 의미 없음
	if (Duration <= 0.0f || Elapsed >= Duration - 0.05f) return;

	//소스 자신의 진행 시간 기준 진행률 (PrepareRootMotion의 MoveFraction과 동일 기준)
	const float Alpha = Elapsed / Duration;
	const float OneMinusAlpha = 1.0f - Alpha;

	//분모 안전 가드 (위 0.05초 가드로 사실상 도달 불가지만 방어)
	if (OneMinusAlpha < KINDA_SMALL_NUMBER) return;

	const TSharedPtr<FRootMotionSource_MoveToDynamicForce> MoveSource = StaticCastSharedPtr<FRootMotionSource_MoveToDynamicForce>(Source);

	//현재 lerp 직선상 진행 위치 (HeightCurve 영향 제외한 그라운드 위치)
	const FVector OldLerpGround = FMath::Lerp(MoveSource->StartLocation, MoveSource->TargetLocation, Alpha);

	//새 직선이 OldLerpGround를 정확히 지나도록 NewStart 역산
	//→ 재타게팅 직후 lerp 목표점이 그대로 유지되므로 시각적 도약 0
	const FVector NewStart = (OldLerpGround - Alpha * CachedDestination) / OneMinusAlpha;

	//소스 교체 없이 같은 인스턴스의 StartLocation(setter 없음, 직접 대입)과 TargetLocation 갱신
	//MoveToDynamicForce::Matches는 Start/Target을 비교하지 않음 → 같은 소스 수정이 시뮬프록시 ID 매칭에 안전
	//UpdateStateFrom이 StartLocation/TargetLocation을 둘 다 복사 → 시뮬프록시에도 전달
	//PathOffset 기준 방향(Target − Start)이 바뀌므로 HeightCurve의 X/Y 성분은 회전될 수 있음 (Z 성분은 영향 없음)
	//InitialTargetLocation은 최초 목적지 유지 (변경 금지)
	MoveSource->StartLocation = NewStart;
	MoveSource->SetTargetLocation(CachedDestination);

	DEBUG_LOG(TEXT("OnEventTrackingTarget: Alpha=%.3f, NewStart=%s, Dest=%s"),
		Alpha, *NewStart.ToString(), *CachedDestination.ToString());
}

void UEnemyLungeAbility::OnEventLungeStart(FGameplayEventData Payload)
{
	//Lunging ANS Begin — EventMagnitude = ANS 지속시간 (이동 시간)
	float Duration = Payload.EventMagnitude;
	DEBUG_LOG(TEXT("OnEventLungeStart: Duration=%.2f, Destination=%s"), Duration, *CachedDestination.ToString());

	StartLungeMovement(Duration);
}

void UEnemyLungeAbility::OnEventLungeEnd(FGameplayEventData Payload)
{
	//Lunging ANS End — RootMotionSource 정리
	DEBUG_LOG(TEXT("OnEventLungeEnd"));

	StopLungeMovement();
}

#pragma endregion

#pragma region "Movement"

void UEnemyLungeAbility::StartLungeMovement(float Duration)
{
	AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo();
	if (!Enemy || !CachedLungeConfig)
	{
		DEBUG_LOG(TEXT("StartLungeMovement: Enemy or LungeConfig is nullptr"));
		return;
	}
	UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement();
	if (!CMC) return;

	//LungeStart 중복 수신 시 이전 소스가 CMC에 남지 않도록 정리
	if (IsLungeActive())
	{
		StopLungeMovement();
	}

	Duration = FMath::Max(Duration, 0.05f);

	//소스 등록은 이동 시작 시 1회. 이후 재타게팅은 같은 인스턴스의 Start/Target 갱신으로 처리
	const FVector StartLocation = Enemy->GetActorLocation();

	DEBUG_LOG(TEXT("StartLungeMovement: Start=%s, Dest=%s, Duration=%.2f"),
		*StartLocation.ToString(),
		*CachedDestination.ToString(),
		Duration);

	CurrentLungeSourceID = AddLungeRootMotionSource(
		CMC,
		StartLocation,
		CachedDestination,
		Duration);
}

void UEnemyLungeAbility::StopLungeMovement()
{
	if (CurrentLungeSourceID == 0) return;

	if (AEnemyCharacter* Enemy = GetEnemyCharacterFromActorInfo())
	{
		if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
		{
			CMC->RemoveRootMotionSourceByID(CurrentLungeSourceID);
		}
	}
	CurrentLungeSourceID = 0;
}

bool UEnemyLungeAbility::IsLungeActive() const
{
	return CurrentLungeSourceID != 0;
}

uint16 UEnemyLungeAbility::AddLungeRootMotionSource(
	UCharacterMovementComponent* CMC,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	float Duration)
{
	if (!CMC || !CachedLungeConfig) return 0;

	TSharedPtr<FRootMotionSource_MoveToDynamicForce> Source = MakeShared<FRootMotionSource_MoveToDynamicForce>();
	Source->InstanceName = FName("LungeMove");
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	Source->Priority = 1000;
	Source->StartLocation = StartLocation;
	//최초 목적지 (이후 변경 금지, 네트 직렬화용)
	Source->InitialTargetLocation = TargetLocation;
	Source->TargetLocation = TargetLocation;
	Source->Duration = Duration;
	Source->bRestrictSpeedToExpected = false;
	Source->PathOffsetCurve = CachedLungeConfig->HeightCurve;
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;

	return CMC->ApplyRootMotionSource(Source);
}

#pragma endregion

#pragma region "Override Functions"

void UEnemyLungeAbility::OnActionRecoveryEnd()
{
	//Lunge 콤보(인덱스 0)는 CheckCondition AN이 없으므로
	//ComboCounter가 증가하지 않고 bPerformNextCombo가 초기값(true) 그대로 남음
	//무한 루프 방지를 위해 직접 ComboCounter를 증가시키고 후속 콤보 시도
	if (ComboCounter == 0)
	{
		ComboCounter++;

		if (ComboCounter < MaxComboCount)
		{
			DEBUG_LOG(TEXT("OnActionRecoveryEnd: Lunge done → starting follow-up combo (ComboCounter=%d)"), ComboCounter);
			bPerformNextCombo = true;
			ExecuteAttack();
		}
		else
		{
			DEBUG_LOG(TEXT("OnActionRecoveryEnd: Lunge done — no follow-up combos"));
			//후속 콤보 없음 → 몽타주 완료가 EndAbility 처리
		}
	}
	else
	{
		//후속 콤보(인덱스 1+): 부모의 일반 콤보 흐름 (CheckCondition 기반)
		DEBUG_LOG(TEXT("OnActionRecoveryEnd: Follow-up combo — delegating to parent"));
		Super::OnActionRecoveryEnd();
	}
}

#pragma endregion
