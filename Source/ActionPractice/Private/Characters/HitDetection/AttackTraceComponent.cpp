// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/HitDetection/AttackTraceComponent.h"
#include "Items/AttackData.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAttackTraceComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAttackTraceComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UAttackTraceComponent::UAttackTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAttackTraceComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedASC = GetOwnerASC();
	SetOwnerMesh();
}

void UAttackTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsTracing) return;

	//각 소켓 그룹별로 독립적으로 적응형 트레이스 처리
	for (TPair<FName, FHitSocketGroupConfig>& Pair : UsingHitSocketGroups)
	{
		FHitSocketGroupConfig& SocketGroup = Pair.Value;

		//적응형 트레이스 설정 업데이트
		UpdateAdaptiveTraceSettings(SocketGroup);

		//무기 끝 위치 업데이트 (속도 계산용)
		SocketGroup.PrevTipSocketLocation = GetTipSocketLocation(SocketGroup);

		//각 그룹의 TraceAccumulator 업데이트
		SocketGroup.TraceAccumulator += DeltaTime;

		//각 그룹의 TraceAccumulator가 임계값 넘으면 해당 그룹만 트레이스
		if (SocketGroup.TraceAccumulator >= SocketGroup.CurrentSecondsPerTrace)
		{
			PerformTrace(SocketGroup.TraceAccumulator);
			SocketGroup.TraceAccumulator = 0.0f;
		}
	}
}

#pragma region "Event Functions"
void UAttackTraceComponent::BindEventCallbacks()
{
	if (!CachedASC)
	{
		DEBUG_LOG(TEXT("No ASC found"));
		return;
	}

	if (HitDetectionStartHandle.IsValid() || HitDetectionEndHandle.IsValid())
	{
		DEBUG_LOG(TEXT("BindEventCallbacks: Clearing previous handles before binding new"));
		UnbindEventCallbacks();
	}

	DEBUG_LOG(TEXT("BindEventCallbacks: Subscribing events"));

	//HitDetectionStart 노티파이 스테이트
	HitDetectionStartHandle = CachedASC->GenericGameplayEventCallbacks
	.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag())
	.AddLambda([this](const FGameplayEventData* EventData)
	{
		if (IsValid(this) && EventData)
		{
			HandleHitDetectionStart(*EventData);
		}
	});

	//HitDetectionEnd 노티파이 스테이트
	HitDetectionEndHandle = CachedASC->GenericGameplayEventCallbacks
	.FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag())
	.AddLambda([this](const FGameplayEventData* EventData)
	{
		if (IsValid(this) && EventData)
		{
			HandleHitDetectionEnd(*EventData);
		}
	});
}

void UAttackTraceComponent::UnbindEventCallbacks()
{
	if (CachedASC)
	{
		if (HitDetectionStartHandle.IsValid())
		{
			if (FGameplayEventMulticastDelegate* Delegate = CachedASC->GenericGameplayEventCallbacks.Find(UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag()))
			{
				Delegate->Remove(HitDetectionStartHandle);
			}
		}

		if (HitDetectionEndHandle.IsValid())
		{
			if (FGameplayEventMulticastDelegate* Delegate = CachedASC->GenericGameplayEventCallbacks.Find(UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag()))
			{
				Delegate->Remove(HitDetectionEndHandle);
			}
		}
	}

	HitDetectionStartHandle.Reset();
	HitDetectionEndHandle.Reset();
}

void UAttackTraceComponent::HandleHitDetectionStart(const FGameplayEventData& Payload)
{
	if (!bIsPrepared)
	{
		DEBUG_LOG(TEXT("HitDetectionStart - Not Prepared"));
		return;
	}

	float Duration = Payload.EventMagnitude;

	DEBUG_LOG(TEXT("HitDetectionStart Event Received - Duration: %.2f"), Duration);

	StartTrace();
}

void UAttackTraceComponent::HandleHitDetectionEnd(const FGameplayEventData& Payload)
{
	DEBUG_LOG(TEXT("HitDetectionEnd Event Received"));

	StopTrace();
	bIsPrepared = false;
}
#pragma endregion

#pragma region "Trace Config Functions"
void UAttackTraceComponent::PrepareHitDetection(const FGameplayTagContainer& AttackTags, const int32 ComboIndex)
{
	//자식 클래스에서 설정 로드
	if (!LoadTraceConfig(AttackTags, ComboIndex))
	{
		DEBUG_LOG(TEXT("Failed to load trace config for attack tags"));
		return;
	}

	ResetHitActors();
	BindEventCallbacks();

	bIsPrepared = true;

	DEBUG_LOG(TEXT("PrepareHitDetection - Attack Tags Count: %d, Combo: %d"), AttackTags.Num(), ComboIndex);
}

void UAttackTraceComponent::PrepareHitDetection(const FName& AttackName, const int32 ComboIndex)
{
	//자식 클래스에서 설정 로드
	if (!LoadTraceConfig(AttackName, ComboIndex))
	{
		DEBUG_LOG(TEXT("Failed to load trace config for attack name"));
		return;
	}

	ResetHitActors();
	BindEventCallbacks();

	bIsPrepared = true;

	DEBUG_LOG(TEXT("PrepareHitDetection - Attack Name: %s, Combo: %d"), *AttackName.ToString(), ComboIndex);
}

void UAttackTraceComponent::BuildSocketConfigs(const TArray<FHitSocketInfo>& SocketInfoArray)
{
	PrebuiltSocketGroups.Empty();

	for (const FHitSocketInfo& SocketInfo : SocketInfoArray)
	{
		FHitSocketGroupConfig SocketConfig;
		SocketConfig.SocketGroupName = SocketInfo.HitSocketName;
		SocketConfig.SocketCount = SocketInfo.HitSocketCount;
		SocketConfig.TraceRadius = 10.0f; //기본값, 개별 공격에서 설정됨
		SocketConfig.AttackMotionType = EAttackDamageType::None; //공격마다 설정됨

		//소켓 이름들 미리 생성: prefix_0, prefix_1, prefix_2 ...
		SocketConfig.TraceSocketNames.Empty();
		for (int32 i = 0; i < SocketInfo.HitSocketCount; ++i)
		{
			const FString FullName = FString::Printf(TEXT("%s_%d"), *SocketInfo.HitSocketName.ToString(), i);
			SocketConfig.TraceSocketNames.Add(FName(*FullName));
		}

		//위치 배열 초기화
		SocketConfig.PreviousSocketPositions.SetNum(SocketInfo.HitSocketCount);
		SocketConfig.CurrentSocketPositions.SetNum(SocketInfo.HitSocketCount);

		PrebuiltSocketGroups.Add(SocketInfo.HitSocketName, SocketConfig);
		DEBUG_LOG(TEXT("Prebuilt socket config: %s (Count: %d)"), *SocketInfo.HitSocketName.ToString(), SocketInfo.HitSocketCount);
	}
}
#pragma endregion
	
#pragma region "Trace Functions"
void UAttackTraceComponent::StartTrace()
{
	if (bIsTracing)
	{
		DEBUG_LOG(TEXT("Already tracing, stopping previous trace"));
		StopTrace();
	}

	//초기 소켓 위치
	if (!UpdateSocketPositions())
	{
		DEBUG_LOG(TEXT("Failed to update socket positions - no valid sockets found"));
		return;
	}

	//각 그룹의 Previous 위치를 Current로 초기화 및 적응형 트레이스 변수 초기화
	for (TPair<FName, FHitSocketGroupConfig>& Pair : UsingHitSocketGroups)
	{
		Pair.Value.PreviousSocketPositions = Pair.Value.CurrentSocketPositions;
		Pair.Value.PrevTipSocketLocation = GetTipSocketLocation(Pair.Value);
		Pair.Value.TraceAccumulator = 0.0f;
	}

	bIsTracing = true;
	SetComponentTickEnabled(true);

	DebugSweepTraceCounter = 0;
	DEBUG_LOG(TEXT("Started trace"));
}

void UAttackTraceComponent::StopTrace()
{
	bIsTracing = false;
	SetComponentTickEnabled(false);
	UnbindEventCallbacks();

	DEBUG_LOG(TEXT("Stopped trace, counter: %d"), DebugSweepTraceCounter);
}

void UAttackTraceComponent::PerformTrace(float DeltaTime)
{
	if (UsingHitSocketGroups.Num() == 0)
	{
		DEBUG_LOG(TEXT("PerformTrace - FAILED: No socket groups"));
		return;
	}

	if (!UpdateSocketPositions())
	{
		DEBUG_LOG(TEXT("PerformTrace - FAILED: cannot update socket positions during trace"));
		StopTrace();
		return;
	}

	//모든 소켓 그룹에 대해 트레이스 수행
	for (TPair<FName, FHitSocketGroupConfig>& Pair : UsingHitSocketGroups)
	{
		switch (Pair.Value.AttackMotionType)
		{
		case EAttackDamageType::Slash:
			PerformSlashTrace(Pair.Value);
			break;

		case EAttackDamageType::Pierce:
			PerformPierceTrace(Pair.Value);
			break;

		case EAttackDamageType::Strike:
			PerformStrikeTrace(Pair.Value);
			break;

		default:
			DEBUG_LOG(TEXT("Unknown damage type: %d"), (int32)Pair.Value.AttackMotionType);
			break;
		}
	}

	//각 그룹의 이전 소켓위치를 현재 소켓위치로 변경
	for (TPair<FName, FHitSocketGroupConfig>& Pair : UsingHitSocketGroups)
	{
		Pair.Value.PreviousSocketPositions = Pair.Value.CurrentSocketPositions;
	}
}

bool UAttackTraceComponent::UpdateSocketPositions()
{
	if (!OwnerMesh)
	{
		return false;
	}

	//모든 소켓 그룹의 위치 업데이트
	for (TPair<FName, FHitSocketGroupConfig>& Pair : UsingHitSocketGroups)
	{
		Pair.Value.CurrentSocketPositions.Empty();
		for (const FName& SocketName : Pair.Value.TraceSocketNames)
		{
			if (OwnerMesh->DoesSocketExist(SocketName))
			{
				FVector SocketLocation = OwnerMesh->GetSocketLocation(SocketName);
				Pair.Value.CurrentSocketPositions.Add(SocketLocation);
			}
			else
			{
				DEBUG_LOG(TEXT("Socket %s not found on mesh"), *SocketName.ToString());
				Pair.Value.CurrentSocketPositions.Empty();
				return false;
			}
		}
	}
	return true;
}

void UAttackTraceComponent::PerformPierceTrace(FHitSocketGroupConfig& SocketGroup)
{

}

void UAttackTraceComponent::PerformStrikeTrace(FHitSocketGroupConfig& SocketGroup)
{

}

void UAttackTraceComponent::PerformSlashTrace(FHitSocketGroupConfig& SocketGroup)
{
	if (SocketGroup.CurrentSocketPositions.Num() < 2)
	{
		DEBUG_LOG(TEXT("PerformSlashTrace - FAILED: Not enough socket positions (need >= 2)"));
		return;
	}

	TArray<FHitResult> AllHits;

	//소켓 이름 순서대로 트레이스 시작과 끝 설정 (0 ~ 1, 1 ~ 2, ...)
	for (int32 i = 0; i < SocketGroup.CurrentSocketPositions.Num() - 1; i++)
	{
		FVector StartPrev = SocketGroup.PreviousSocketPositions[i];
		FVector StartCurr = SocketGroup.CurrentSocketPositions[i];

		FVector EndPrev = SocketGroup.PreviousSocketPositions[i + 1];
		FVector EndCurr = SocketGroup.CurrentSocketPositions[i + 1];

		TArray<FHitResult> SubHits;

		PerformInterpolationTrace(StartPrev, StartCurr, EndPrev, EndCurr, SocketGroup.TraceRadius, SocketGroup.CurrentInterpolationPerTrace, SubHits);
		AllHits.Append(SubHits);
	}

	for (const FHitResult& Hit : AllHits)
	{
		//일단 다단히트가 아닌 일반 공격으로 호출
		if (ValidateHit(Hit.GetActor(), Hit, false))
		{
			ProcessHit(Hit.GetActor(), Hit);
		}
	}
}
#pragma endregion

#pragma region "Hit Functions"
bool UAttackTraceComponent::ValidateHit(AActor* HitActor, const FHitResult& HitResult, bool bIsMultiHit)
{
	if (!HitActor)
		return false;

	//자기 자신과 소유자 제외
	AActor* Owner = GetOwnerActor();
	if (HitActor == Owner) return false;

	//필요 시 아군 제외 구현해야 함

	//중복 체크
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (FHitValidationData* ValidationData = HitValidationMap.Find(HitActor))
	{
		//다단히트가 아닐 경우, 이미 있으면 리턴
		if (!bIsMultiHit) return false;

		if (CurrentTime - ValidationData->LastHitTime < HitCooldownTime)
		{
			return false;
		}

		ValidationData->LastHitTime = CurrentTime;
		ValidationData->HitCount++;
	}
	else
	{
		FHitValidationData NewData;
		NewData.HitActor = HitActor;
		NewData.LastHitTime = CurrentTime;
		NewData.HitCount = 1;
		HitValidationMap.Add(HitActor, NewData);
	}

	return true;
}

void UAttackTraceComponent::ProcessHit(AActor* HitActor, const FHitResult& HitResult)
{
	float IncomingDamage = CurrentAttackData.FinalDamage;

	DEBUG_LOG(TEXT("Hit %s, IncomingDamage: %.2f"), *HitActor->GetName(), IncomingDamage);

	//화면에 디버그 메시지 표시
	if (GEngine)
	{
		//데미지 타입별 색상 설정
		FColor MessageColor = FColor::Green;
		FString DamageTypeName = TEXT("Unknown");

		//히트 메시지 출력
		GEngine->AddOnScreenDebugMessage(
			-1,  //키 (-1은 중복 허용)
			2.0f,  //표시 시간 (초)
			MessageColor,  //색상
			FString::Printf(TEXT("HIT %s, Damage x%.2f"),
				*HitActor->GetName(),
				IncomingDamage)
		);
	}

	OnHit.Broadcast(HitActor, HitResult, CurrentAttackData);
}

void UAttackTraceComponent::ResetHitActors()
{
	HitValidationMap.Empty();
	DEBUG_LOG(TEXT("Reset hit actors"));
}

void UAttackTraceComponent::AddIgnoredActors(FCollisionQueryParams& Params) const
{
	AActor* Owner = GetOwnerActor();
	if (Owner)
	{
		Params.AddIgnoredActor(Owner);
	}
}

#pragma endregion

#pragma region "Adaptive Trace Functions"
FVector UAttackTraceComponent::GetTipSocketLocation(const FHitSocketGroupConfig& SocketGroup) const
{
	if (!OwnerMesh) return FVector::ZeroVector;

	//파라미터로 받은 SocketGroup의 첫 번째 소켓을 TipSocket으로 사용
	if (SocketGroup.TraceSocketNames.Num() == 0) return FVector::ZeroVector;

	return OwnerMesh->GetSocketLocation(SocketGroup.TraceSocketNames[0]);
}

float UAttackTraceComponent::CalculateSwingSpeed(const FHitSocketGroupConfig& SocketGroup) const
{
	if (!GetWorld()) return 0.0f;

	//속도 = 거리 / 시간이므로 DeltaTime이 필요
	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	if (DeltaTime <= KINDA_SMALL_NUMBER) //0으로 나누기 방지
	{
		return 0.0f;
	}

	FVector CurTipSocketLocation = GetTipSocketLocation(SocketGroup);

	//속도 (cm/s) 계산
	const float DistanceTraveled = FVector::Dist(CurTipSocketLocation, SocketGroup.PrevTipSocketLocation);
	const float SpeedInCmPerSecond = DistanceTraveled / DeltaTime;

	return SpeedInCmPerSecond;
}

void UAttackTraceComponent::UpdateAdaptiveTraceSettings(FHitSocketGroupConfig& SocketGroup)
{
	float SwingSpeed = CalculateSwingSpeed(SocketGroup);

	//속도에 따른 설정 선택
	FAdaptiveTraceConfig SelectedConfig = AdaptiveConfigs[0];
	int32 i = 0;
	for (i; i < AdaptiveConfigs.Num(); ++i)
	{
		if (SwingSpeed >= AdaptiveConfigs[i].SpeedThreshold)
		{
			SelectedConfig = AdaptiveConfigs[i];
		}
		else
		{
			break;
		}
	}

	SocketGroup.CurrentSecondsPerTrace = SelectedConfig.SecondsPerTrace;
	SocketGroup.CurrentInterpolationPerTrace = SelectedConfig.InterpolationPerTrace;
}

void UAttackTraceComponent::PerformInterpolationTrace(
	const FVector& StartPrev, const FVector& StartCurr,
	const FVector& EndPrev, const FVector& EndCurr,
	float Radius, int32 InterpolationPerTrace, TArray<FHitResult>& OutHits)
{
	//이전 프레임 소켓 위치와 현재 프레임 소켓 위치 사이의 간극이 큰 것을 방지하기 위해
	//보간으로 프레임 간 중간 지점을 찾아 스윕 포인트 추가

	FCollisionQueryParams Params = GetCollisionQueryParams();

	for (int32 i = 0; i <= InterpolationPerTrace; ++i)
	{
		float Alpha = static_cast<float>(i) / static_cast<float>(InterpolationPerTrace);

		FVector InterpStart = FMath::Lerp(StartPrev, StartCurr, Alpha);
		FVector InterpEnd = FMath::Lerp(EndPrev, EndCurr, Alpha);

		TArray<FHitResult> SubHits;

		GetWorld()->SweepMultiByChannel(
			SubHits,
			InterpStart,
			InterpEnd,
			FQuat::Identity,
			GetTraceChannel(),
			FCollisionShape::MakeCapsule(Radius, (InterpEnd - InterpStart).Size() * 0.5f),
			Params
		);

		OutHits.Append(SubHits);

		++DebugSweepTraceCounter;
		if (bDrawDebugTrace)
		{
			DrawDebugCapsule(GetWorld(),
			                 (InterpStart + InterpEnd) * 0.5f,
			                 (InterpEnd - InterpStart).Size() * 0.5f,
			                 Radius,
			                 FQuat::FindBetweenNormals(FVector::UpVector, (InterpEnd - InterpStart).GetSafeNormal()),
			                 DebugTraceColor,
			                 false,
			                 DebugTraceDuration);
		}
	}
}
#pragma endregion

#pragma region "Utility Functions"
ECollisionChannel UAttackTraceComponent::GetTraceChannel() const
{
	//프로젝트 설정에서 커스텀 채널 사용
	//DefaultEngine.ini에서 설정 필요
	return ECC_GameTraceChannel1;  //WeaponTrace 채널
}

FCollisionQueryParams UAttackTraceComponent::GetCollisionQueryParams() const
{
	FCollisionQueryParams Params(TEXT("AttackTrace"), false);

	AddIgnoredActors(Params);

	return Params;
}

#pragma endregion

void UAttackTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindEventCallbacks();

	Super::EndPlay(EndPlayReason);
}

#pragma region "Debug And Profiling"
void UAttackTraceComponent::DrawDebugSweepTrace(
	const FVector& StartPrev, const FVector& StartCurr,
	const FVector& EndPrev, const FVector& EndCurr,
	float Radius, const FColor& Color)
{
	//사다리꼴 와이어프레임 그리기
	DrawDebugLine(GetWorld(), StartPrev, EndPrev, Color, false, DebugTraceDuration);
	DrawDebugLine(GetWorld(), StartCurr, EndCurr, Color, false, DebugTraceDuration);
	DrawDebugLine(GetWorld(), StartPrev, StartCurr, Color, false, DebugTraceDuration);
	DrawDebugLine(GetWorld(), EndPrev, EndCurr, Color, false, DebugTraceDuration);

	//모서리에 구체 그리기
	DrawDebugSphere(GetWorld(), StartPrev, Radius, 8, Color, false, DebugTraceDuration);
	DrawDebugSphere(GetWorld(), StartCurr, Radius, 8, Color, false, DebugTraceDuration);
	DrawDebugSphere(GetWorld(), EndPrev, Radius, 8, Color, false, DebugTraceDuration);
	DrawDebugSphere(GetWorld(), EndCurr, Radius, 8, Color, false, DebugTraceDuration);
}

void UAttackTraceComponent::ToggleWeaponDebugTrace()
{
	bDrawDebugTrace = !bDrawDebugTrace;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(100, 2.0f, bDrawDebugTrace ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("Debug: %s"), bDrawDebugTrace ? TEXT("ON") : TEXT("OFF")));
	}
}
#pragma endregion
