#include "Public/Characters/LockOnComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/LockOn/LockOnTargetSelector.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogLockOnComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogLockOnComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ULockOnComponent::ULockOnComponent()
{
	SetIsReplicatedByDefault(true);
}

void ULockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	//로컬 플레이어에게만 마커 위젯 컴포넌트 생성
	APawn* OwnerPawn = GetOwner<APawn>();
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;
	if (!LockOnMarkerWidgetClass) return;

	LockOnMarkerWidget = NewObject<UWidgetComponent>(GetOwner(), UWidgetComponent::StaticClass(), TEXT("LockOnMarker"));
	LockOnMarkerWidget->SetWidgetClass(LockOnMarkerWidgetClass);
	LockOnMarkerWidget->SetWidgetSpace(EWidgetSpace::Screen);
	LockOnMarkerWidget->SetDrawSize(LockOnMarkerDrawSize);
	LockOnMarkerWidget->SetVisibility(false);
	LockOnMarkerWidget->RegisterComponent();
	LockOnMarkerWidget->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
}

void ULockOnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULockOnComponent, bIsLockOn);
	DOREPLIFETIME(ULockOnComponent, LockedOnTarget);
}

void ULockOnComponent::SetLockedOnTarget(AActor* NewTarget)
{
	//이전 타겟 HP바 해제
	AActor* PreviousTarget = LockedOnTarget;

	bIsLockOn = (NewTarget != nullptr);
	LockedOnTarget = NewTarget;

	if (bIsLockOn)
	{
		SetRotationMode(false, true);
		ShowLockOnMarker(NewTarget);
		StartLockOnMaintenance();

		//새 타겟이 EnemyCharacter이면 HP바 표시
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(NewTarget))
		{
			Enemy->SetLockedOnByPlayer(true);
		}

		DEBUG_LOG(TEXT("Lock-On Target: %s"), *NewTarget->GetName());
	}
	else
	{
		SetRotationMode(true, false);
		HideLockOnMarker();
		StopLockOnMaintenance();
		DEBUG_LOG(TEXT("Lock-On Released"));
	}

	//이전 타겟 HP바 락온 해제 (새 타겟과 다를 때만)
	if (PreviousTarget && PreviousTarget != NewTarget)
	{
		if (AEnemyCharacter* PrevEnemy = Cast<AEnemyCharacter>(PreviousTarget))
		{
			PrevEnemy->SetLockedOnByPlayer(false);
		}
	}

	if (!GetOwner()->HasAuthority())
	{
		ServerSetLockOnState(bIsLockOn, LockedOnTarget);
	}
}

void ULockOnComponent::ServerSetLockOnState_Implementation(bool bNewLockOn, AActor* NewTarget)
{
	//해제 요청은 항상 수용
	if (!bNewLockOn || !NewTarget)
	{
		bIsLockOn = false;
		LockedOnTarget = nullptr;
		return;
	}

	if (!ValidateLockOnRequest(NewTarget))
	{
		//검증 실패 시 서버 상태 변경 X
		DEBUG_LOG(TEXT("Server: Lock-On request rejected - Target: %s"), *NewTarget->GetName());
		return;
	}

	bIsLockOn = bNewLockOn;
	LockedOnTarget = NewTarget;

	DEBUG_LOG(TEXT("Server: Lock-On State Updated - bIsLockOn: %s, Target: %s"),
		bNewLockOn ? TEXT("true") : TEXT("false"),
		NewTarget ? *NewTarget->GetName() : TEXT("None"));
}

bool ULockOnComponent::ValidateLockOnRequest(const AActor* NewTarget) const
{
	if (!IsValid(NewTarget)) return false;

	const AActor* Owner = GetOwner();
	if (!Owner) return false;

	if (!NewTarget->ActorHasTag(TargetActorTag)) return false;

	const float MaxAllowedDistance = LockOnMaxDistance * ReleaseDistanceMultiplier + ServerValidationTolerance;

	if (FVector::Dist(Owner->GetActorLocation(), NewTarget->GetActorLocation()) > MaxAllowedDistance) return false;

	const FGameplayTag DeadTag = UGameplayTagsSubsystem::GetStateDeadTag();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(NewTarget));

	if (TargetASC && DeadTag.IsValid() && TargetASC->HasMatchingGameplayTag(DeadTag)) return false;

	return true;
}

void ULockOnComponent::SetRotationMode(bool bOrientToMovement, bool bUseControllerDesired)
{
	ACharacter* OwnerCharacter = GetOwner<ACharacter>();
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) return;

	CMC->bOrientRotationToMovement = bOrientToMovement;
	CMC->bUseControllerDesiredRotation = bUseControllerDesired;

	if (bCachedOrientToMovement != bOrientToMovement || bCachedUseControllerDesired != bUseControllerDesired)
	{
		bCachedOrientToMovement = bOrientToMovement;
		bCachedUseControllerDesired = bUseControllerDesired;

		if (!OwnerCharacter->HasAuthority())
		{
			ServerSetRotationMode(bOrientToMovement, bUseControllerDesired);
		}
	}
}

void ULockOnComponent::ServerSetRotationMode_Implementation(bool bOrientToMovement, bool bUseControllerDesired)
{
	ACharacter* OwnerCharacter = GetOwner<ACharacter>();
	if (!OwnerCharacter) return;

	UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) return;

	CMC->bOrientRotationToMovement = bOrientToMovement;
	CMC->bUseControllerDesiredRotation = bUseControllerDesired;

	DEBUG_LOG(TEXT("Server: RotationMode Updated - OrientToMovement: %s, UseControllerDesired: %s"),
		bOrientToMovement ? TEXT("true") : TEXT("false"),
		bUseControllerDesired ? TEXT("true") : TEXT("false"));
}

#pragma region "Target Search"

bool ULockOnComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = GetOwner<APawn>();
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

bool ULockOnComponent::BuildLockOnView(FLockOnView& OutView) const
{
	const APawn* OwnerPawn = GetOwner<APawn>();
	if (!OwnerPawn) return false;

	//카메라 기준 판정이므로 로컬클라에서만 유효
	const APlayerController* PC = OwnerPawn->GetController<APlayerController>();
	if (!PC || !PC->IsLocalController()) return false;

	const APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager) return false;

	const FRotator CameraRotation = CameraManager->GetCameraRotation();
	const FRotationMatrix CameraBasis(CameraRotation);

	OutView.Location = CameraManager->GetCameraLocation();
	OutView.Forward = CameraBasis.GetUnitAxis(EAxis::X);
	OutView.Right = CameraBasis.GetUnitAxis(EAxis::Y);
	OutView.Up = CameraBasis.GetUnitAxis(EAxis::Z);

	return true;
}

FLockOnScoringParams ULockOnComponent::MakeScoringParams() const
{
	FLockOnScoringParams Params;
	Params.MaxDistance = LockOnMaxDistance;
	Params.MaxAngleDegrees = MaxAngleDegrees;
	Params.AngleWeight = AngleWeight;
	Params.DistanceWeight = DistanceWeight;

	return Params;
}

FVector ULockOnComponent::GetAimPoint(const AActor* Target) const
{
	if (!IsValid(Target)) return FVector::ZeroVector;

	//ShowLockOnMarker의 소켓 판정과 동일한 규칙을 사용해 마커 위치와 판정 기준점을 일치
	if (!LockOnMarkerSocketName.IsNone())
	{
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(Target))
		{
			const USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();

			if (Mesh && Mesh->DoesSocketExist(LockOnMarkerSocketName))
			{
				return Mesh->GetSocketLocation(LockOnMarkerSocketName);
			}
		}
	}

	return Target->GetActorLocation();
}

void ULockOnComponent::GatherCandidates(TArray<FLockOnCandidate>& OutCandidates, const AActor* ExcludedActor) const
{
	OutCandidates.Reset();

	const AActor* Owner = GetOwner();
	if (!Owner) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	//GetAllActorsWithTag 대신 물리 씬 BVH에 위임한 반경 질의 사용
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LockOnGatherCandidates), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(LockOnMaxDistance), QueryParams);

	const FGameplayTag DeadTag = UGameplayTagsSubsystem::GetStateDeadTag();

	//오버랩 결과는 컴포넌트 단위이므로 액터 단위로 중복 제거
	TSet<const AActor*> UniqueActors;
	UniqueActors.Reserve(Overlaps.Num());

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();

		if (!IsValid(Candidate) || Candidate == Owner || Candidate == ExcludedActor) continue;

		if (UniqueActors.Contains(Candidate)) continue;

		if (!Candidate->ActorHasTag(TargetActorTag)) continue;

		//사망한 대상 제외
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate);

		if (TargetASC && DeadTag.IsValid() && TargetASC->HasMatchingGameplayTag(DeadTag)) continue;

		UniqueActors.Add(Candidate);

		FLockOnCandidate NewCandidate;
		NewCandidate.Actor = Candidate;
		NewCandidate.AimPoint = GetAimPoint(Candidate);
		OutCandidates.Add(MoveTemp(NewCandidate));
	}

	DEBUG_LOG(TEXT("GatherCandidates: overlaps=%d, candidates=%d"), Overlaps.Num(), OutCandidates.Num());
}

bool ULockOnComponent::IsTargetVisible(const FLockOnView& View, const FVector& AimPoint) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	const AActor* Owner = GetOwner();

	//지형/구조물만 가림으로 판정. 다른 캐릭터(Pawn)는 가림으로 보지 않음
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LockOnVisibility), true, Owner);

	const bool bBlocked = World->LineTraceTestByObjectType(View.Location, AimPoint, ObjectParams, QueryParams);

	return !bBlocked;
}

AActor* ULockOnComponent::FindBestTarget(const AActor* ExcludedActor)
{
	//카메라가 필요하므로 로컬클라 전용
	if (!IsLocallyControlledOwner()) return nullptr;

	const AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	FLockOnView View;
	if (!BuildLockOnView(View)) return nullptr;

	TArray<FLockOnCandidate> Candidates;
	GatherCandidates(Candidates, ExcludedActor);

	if (Candidates.Num() <= 0) return nullptr;

	const FLockOnScoringParams Params = MakeScoringParams();
	const FVector OwnerLocation = Owner->GetActorLocation();

	//탈락 조건(AND)을 먼저 적용하고, 통과한 후보만 선호 점수로 순위 매김
	TArray<FLockOnCandidate> Scored;
	Scored.Reserve(Candidates.Num());

	for (FLockOnCandidate& Candidate : Candidates)
	{
		if (FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, Candidate))
		{
			Scored.Add(Candidate);
		}
	}

	if (Scored.Num() <= 0) return nullptr;

	//최대 힙으로 우선순위가 높은 것부터 라인트레이싱
	FLockOnTargetSelector::BuildRankedHeap(Scored);

	int32 TraceCount = 0;
	FLockOnCandidate Best;

	while (FLockOnTargetSelector::PopRankedCandidate(Scored, Best))
	{
		AActor* BestActor = Best.Actor.Get();

		if (!IsValid(BestActor)) continue;

		++TraceCount;

		if (IsTargetVisible(View, Best.AimPoint))
		{
			DEBUG_LOG(TEXT("FindBestTarget: %s (Score: %.3f, Traces: %d)"), *BestActor->GetName(), Best.Score, TraceCount);
			return BestActor;
		}
	}

	DEBUG_LOG(TEXT("FindBestTarget: None (Traces: %d)"), TraceCount);

	return nullptr;
}

void ULockOnComponent::ToggleLockOn()
{
	//서버 실행 X
	if (!IsLocallyControlledOwner()) return;

	if (bIsLockOn)
	{
		SetLockedOnTarget(nullptr);
	}
	else
	{
		AActor* BestTarget = FindBestTarget();

		if (BestTarget)
		{
			SetLockedOnTarget(BestTarget);
		}
	}
}

#pragma endregion

#pragma region "Target Switch"

void ULockOnComponent::AccumulateSwitchInput(const FVector2D& LookAxis)
{
	//서버 실행 X
	if (!IsLocallyControlledOwner()) return;

	if (!bIsLockOn || !IsValid(LockedOnTarget)) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();

	//입력이 끊겼다가 다시 들어오면 누적을 리셋
	if (Now - LastSwitchInputTime > SwitchInputResetTime)
	{
		AccumulatedSwitchInput = FVector2D::ZeroVector;
	}

	LastSwitchInputTime = Now;

	//전환 직후 쿨다운 동안은 누적하지 않음
	if (Now - LastSwitchTime < SwitchCooldown)
	{
		AccumulatedSwitchInput = FVector2D::ZeroVector;
		return;
	}

	//X는 화면 오른쪽, Y는 화면 위 방향으로 취급한다
	//Y는 IA_Look 모디파이어에 따라 부호가 달라지므로 기본 감쇠 계수 0으로 사용하지 않는다
	AccumulatedSwitchInput.X += LookAxis.X;
	AccumulatedSwitchInput.Y += LookAxis.Y * SwitchVerticalInputScale;

	if (AccumulatedSwitchInput.Size() < SwitchInputThreshold) return;

	const FVector2D InputDir = AccumulatedSwitchInput.GetSafeNormal();
	AccumulatedSwitchInput = FVector2D::ZeroVector;

	if (InputDir.IsNearlyZero()) return;

	if (TrySwitchTarget(InputDir))
	{
		LastSwitchTime = Now;
	}
}

bool ULockOnComponent::TrySwitchTarget(const FVector2D& InputDir)
{
	AActor* CurrentTarget = LockedOnTarget;
	if (!IsValid(CurrentTarget)) return false;

	const AActor* Owner = GetOwner();
	if (!Owner) return false;

	FLockOnView View;
	if (!BuildLockOnView(View)) return false;

	TArray<FLockOnCandidate> Candidates;
	GatherCandidates(Candidates, CurrentTarget);

	if (Candidates.Num() <= 0) return false;

	const FLockOnScoringParams Params = MakeScoringParams();
	const FVector OwnerLocation = Owner->GetActorLocation();

	//전환 후보도 탈락 조건(거리/각도/카메라 뒤)은 동일하게 통과해야 함
	TArray<FLockOnCandidate> Filtered;
	Filtered.Reserve(Candidates.Num());

	for (FLockOnCandidate& Candidate : Candidates)
	{
		if (FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, Candidate))
		{
			Filtered.Add(Candidate);
		}
	}

	const FVector CurrentAimPoint = GetAimPoint(CurrentTarget);
	const int32 MaxIterations = FMath::Max(1, SwitchVisibilityMaxIterations);

	//탈락이 없는 단일 최적값 문제이므로 argmin 선형 탐색을 쓰고, 가려진 후보만 빼고 재시도한다
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const int32 BestIndex = FLockOnTargetSelector::SelectSwitchTarget(View, Filtered, CurrentAimPoint, InputDir, SwitchDistancePenaltyWeight);

		if (BestIndex == INDEX_NONE) break;

		AActor* BestActor = Filtered[BestIndex].Actor.Get();

		if (IsValid(BestActor) && IsTargetVisible(View, Filtered[BestIndex].AimPoint))
		{
			DEBUG_LOG(TEXT("TrySwitchTarget: %s (Iteration: %d)"), *BestActor->GetName(), Iteration);
			SetLockedOnTarget(BestActor);
			return true;
		}

		//순서를 유지해야 동점 규칙(먼저 나온 인덱스)이 깨지지 않으므로 RemoveAtSwap을 쓰지 않는다
		Filtered.RemoveAt(BestIndex);
	}

	//해당 방향에 유효한 후보가 없으면 현재 타겟을 유지한다 (반대편 순환 없음)
	return false;
}

#pragma endregion

#pragma region "Lock Maintenance"

void ULockOnComponent::StartLockOnMaintenance()
{
	//카메라/트레이스 기반 검사이므로 로컬클라만
	if (!IsLocallyControlledOwner()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	LostSightAccumulatedTime = 0.0f;

	const float Interval = FMath::Max(0.01f, LockOnMaintenanceInterval);
	World->GetTimerManager().SetTimer(LockOnMaintenanceTimerHandle, this, &ULockOnComponent::TickLockOnMaintenance, Interval, true);
}

void ULockOnComponent::StopLockOnMaintenance()
{
	LostSightAccumulatedTime = 0.0f;

	UWorld* World = GetWorld();
	if (!World) return;

	World->GetTimerManager().ClearTimer(LockOnMaintenanceTimerHandle);
}

void ULockOnComponent::TickLockOnMaintenance()
{
	if (!IsLocallyControlledOwner())
	{
		StopLockOnMaintenance();
		return;
	}

	if (!bIsLockOn)
	{
		StopLockOnMaintenance();
		return;
	}

	AActor* Target = LockedOnTarget;

	//타겟 무효 판정은 컴포넌트로 일원화
	if (!IsValid(Target))
	{
		DEBUG_LOG(TEXT("TickLockOnMaintenance: target invalid, releasing"));
		SetLockedOnTarget(nullptr);
		return;
	}

	const AActor* Owner = GetOwner();

	if (!Owner)
	{
		StopLockOnMaintenance();
		return;
	}

	//획득 거리보다 큰 해제 거리를 써서 경계에서 락/해제 반복 문제 없앰
	const float ReleaseDistance = LockOnMaxDistance * ReleaseDistanceMultiplier;
	const FVector AimPoint = GetAimPoint(Target);

	if (FVector::Dist(Owner->GetActorLocation(), AimPoint) > ReleaseDistance)
	{
		DEBUG_LOG(TEXT("TickLockOnMaintenance: out of release distance, releasing"));
		SetLockedOnTarget(nullptr);
		return;
	}

	FLockOnView View;

	if (!BuildLockOnView(View)) return;

	if (IsTargetVisible(View, AimPoint))
	{
		LostSightAccumulatedTime = 0.0f;
		return;
	}

	LostSightAccumulatedTime += FMath::Max(0.01f, LockOnMaintenanceInterval);

	//유예 시간: 지형에 짧은 시간 가려질 때는 유지하도록
	if (LostSightAccumulatedTime >= LoseSightGraceTime)
	{
		DEBUG_LOG(TEXT("TickLockOnMaintenance: lost sight, releasing"));
		SetLockedOnTarget(nullptr);
	}
}

void ULockOnComponent::HandleTargetLost(AActor* LostTarget)
{
	if (!IsLocallyControlledOwner()) return;

	if (!bIsLockOn) return;

	if (LostTarget && LockedOnTarget != LostTarget) return;

	if (bAutoRetargetOnTargetLost)
	{
		//사망 태그는 몽타주 BlendOut 이후에 붙으므로 태그 필터만으로는 죽어가는 적이 재선택됨
		//반드시 잃은 타겟을 명시 제외해야 함
		AActor* NextTarget = FindBestTarget(LostTarget);

		if (NextTarget)
		{
			DEBUG_LOG(TEXT("HandleTargetLost: retarget to %s"), *NextTarget->GetName());
			SetLockedOnTarget(NextTarget);
			return;
		}
	}

	SetLockedOnTarget(nullptr);
}

#pragma endregion

#pragma region "Lock On Marker"

void ULockOnComponent::ShowLockOnMarker(AActor* Target)
{
	if (!LockOnMarkerWidget || !Target) return;

	//스켈레탈 메시 소켓에 부착 시도, 실패 시 루트 컴포넌트로 폴백
	USceneComponent* AttachTarget = Target->GetRootComponent();
	if (!LockOnMarkerSocketName.IsNone())
	{
		if (ACharacter* TargetCharacter = Cast<ACharacter>(Target))
		{
			USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();
			if (Mesh && Mesh->DoesSocketExist(LockOnMarkerSocketName))
			{
				AttachTarget = Mesh;
			}
		}
	}

	FAttachmentTransformRules AttachRules = FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	LockOnMarkerWidget->AttachToComponent(AttachTarget, AttachRules, LockOnMarkerSocketName);
	LockOnMarkerWidget->SetRelativeLocation(LockOnMarkerOffset);
	LockOnMarkerWidget->SetVisibility(true);
}

void ULockOnComponent::HideLockOnMarker()
{
	if (!LockOnMarkerWidget) return;

	LockOnMarkerWidget->SetVisibility(false);
	LockOnMarkerWidget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	LockOnMarkerWidget->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
}

#pragma endregion

void ULockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLockOnMaintenance();

	Super::EndPlay(EndPlayReason);
}
