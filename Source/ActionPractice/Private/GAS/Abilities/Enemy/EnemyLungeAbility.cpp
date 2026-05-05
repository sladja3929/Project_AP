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
	OriginalLungeStartLocation = FVector::ZeroVector;
	LungeStartWorldTime = 0.0f;
	LungeOriginalDuration = 0.0f;
	LastSourceStartLocation = FVector::ZeroVector;
	LastSourceTargetLocation = FVector::ZeroVector;

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
	UWorld* World = GetWorld();
	if (!World) return;

	const float Elapsed = World->GetTimeSeconds() - LungeStartWorldTime;
	//거의 끝나면 재타게팅 의미 없음
	if (Elapsed >= LungeOriginalDuration - 0.05f) return;

	const float Alpha = Elapsed / LungeOriginalDuration;
	const float OneMinusAlpha = 1.0f - Alpha;

	//분모 안전 가드 (위 0.05초 가드로 사실상 도달 불가지만 방어)
	if (OneMinusAlpha < KINDA_SMALL_NUMBER) return;

	//옛 source의 lerp 직선상 현재 진행 위치 (HeightCurve 영향 제외한 그라운드 위치)
	const FVector OldLerpGround = FMath::Lerp(LastSourceStartLocation, LastSourceTargetLocation, Alpha);

	//새 직선이 OldLerpGround를 정확히 지나도록 NewStart 역산
	//→ 재타게팅 직후 캐릭터 위치가 새 lerp 직선 위에 그대로 놓이므로 시각적 도약 0
	const FVector NewStart = (OldLerpGround - Alpha * CachedDestination) / OneMinusAlpha;

	//같은 source의 TargetLocation in-place 변형 대신 source 인스턴스를 통째로 교체
	//→ 시뮬프록시 ConvertRootMotionServerIDsToLocalIDs의 Matches 충돌(ensure trip) 회피
	//Duration/CurrentTime 보존으로 HeightCurve 궤적 연속성 유지
	//StartLocation은 역산값 사용으로 시각적 도약 제거
	CMC->RemoveRootMotionSourceByID(CurrentLungeSourceID);
	CurrentLungeSourceID = AddLungeRootMotionSource(
		CMC,
		NewStart,
		CachedDestination,
		LungeOriginalDuration,
		Elapsed);
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
	UWorld* World = GetWorld();
	if (!World) return;

	Duration = FMath::Max(Duration, 0.05f);

	//출발점/시작시간/Duration 캐싱 (재타게팅 시 source 재생성에 사용)
	OriginalLungeStartLocation = Enemy->GetActorLocation();
	LungeStartWorldTime = World->GetTimeSeconds();
	LungeOriginalDuration = Duration;

	DEBUG_LOG(TEXT("StartLungeMovement: Start=%s, Dest=%s, Duration=%.2f"),
		*OriginalLungeStartLocation.ToString(),
		*CachedDestination.ToString(),
		Duration);

	CurrentLungeSourceID = AddLungeRootMotionSource(
		CMC,
		OriginalLungeStartLocation,
		CachedDestination,
		Duration,
		0.0f);
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
	float Duration,
	float TimeOffset)
{
	if (!CMC || !CachedLungeConfig) return 0;

	TSharedPtr<FRootMotionSource_MoveToForce> Source = MakeShared<FRootMotionSource_MoveToForce>();
	Source->InstanceName = FName("LungeMove");
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	Source->Priority = 1000;
	Source->StartLocation = StartLocation;
	Source->TargetLocation = TargetLocation;
	Source->Duration = Duration;
	Source->bRestrictSpeedToExpected = false;
	Source->PathOffsetCurve = CachedLungeConfig->HeightCurve;
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;

	//재타게팅 시 시간 보존 (TimeOffset > 0이면 source 내부 시간을 진행된 만큼 앞당김)
	if (TimeOffset > 0.0f)
	{
		Source->SetTime(TimeOffset);
	}

	const uint16 NewID = CMC->ApplyRootMotionSource(Source);

	//역산용 직선 정보 캐싱 (다음 재타게팅 시 OldLerpGround 계산에 사용)
	LastSourceStartLocation = StartLocation;
	LastSourceTargetLocation = TargetLocation;

	return NewID;
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
