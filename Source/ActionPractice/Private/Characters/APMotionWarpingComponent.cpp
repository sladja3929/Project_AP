#include "Characters/APMotionWarpingComponent.h"
#include "GameFramework/Pawn.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogAPMotionWarpingComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogAPMotionWarpingComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

//워프 타깃 이름 상수 정의
const FName UAPMotionWarpingComponent::WarpTargetName_Facing = TEXT("Facing");
const FName UAPMotionWarpingComponent::WarpTargetName_AttackTarget = TEXT("AttackTarget");

UAPMotionWarpingComponent::UAPMotionWarpingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UAPMotionWarpingComponent::SetWarpTargetYaw(FName WarpTargetName, float Yaw)
{
	const AActor* Owner = GetOwner();
	if (!Owner) return;

	//회전 전용 워프이므로 위치는 현재 위치 사용 (ANS에서 Warp Translation off 전제)
	const FRotator TargetRotation(0.0f, FRotator::NormalizeAxis(Yaw), 0.0f);
	AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, Owner->GetActorLocation(), TargetRotation);

	DEBUG_LOG(TEXT("SetWarpTargetYaw: %s Yaw=%.1f HasAuthority=%d"), *WarpTargetName.ToString(), TargetRotation.Yaw, Owner->HasAuthority());
}

void UAPMotionWarpingComponent::SetWarpTargetYawPredicted(FName WarpTargetName, float Yaw)
{
	SetWarpTargetYaw(WarpTargetName, Yaw);

	if (ShouldSendToServer())
	{
		Server_SetWarpTargetYaw(WarpTargetName, Yaw);
	}
}

void UAPMotionWarpingComponent::RemoveWarpTargetPredicted(FName WarpTargetName)
{
	RemoveWarpTarget(WarpTargetName);

	if (ShouldSendToServer())
	{
		Server_RemoveWarpTarget(WarpTargetName);
	}
}

void UAPMotionWarpingComponent::SetWarpTargetFollowActor(FName WarpTargetName, const AActor* TargetActor)
{
	const USceneComponent* TargetComponent = TargetActor ? TargetActor->GetRootComponent() : nullptr;

	if (!TargetComponent)
	{
		RemoveWarpTarget(WarpTargetName);
		DEBUG_LOG(TEXT("SetWarpTargetFollowActor: No target, removed %s"), *WarpTargetName.ToString());
		return;
	}

	//bFollowComponent=true → 워프 윈도우 동안 매 틱 타깃 위치 재조회 (추적 회전)
	//오프셋 인자 기본값만 쓰면 deprecated 예정 오버로드와 모호하므로 LocationOffsetDirection 명시
	AddOrUpdateWarpTargetFromComponent(WarpTargetName, TargetComponent, NAME_None, true, EWarpTargetLocationOffsetDirection::TargetsForwardVector);

	DEBUG_LOG(TEXT("SetWarpTargetFollowActor: %s -> %s"), *WarpTargetName.ToString(), *TargetActor->GetName());
}

void UAPMotionWarpingComponent::Server_SetWarpTargetYaw_Implementation(FName WarpTargetName, float Yaw)
{
	//클라이언트 입력값 검증 (허용 이름 + 유한값)
	if (!IsClientSettableWarpTarget(WarpTargetName) || !FMath::IsFinite(Yaw))
	{
		DEBUG_LOG(TEXT("Server_SetWarpTargetYaw: Rejected %s Yaw=%f"), *WarpTargetName.ToString(), Yaw);
		return;
	}

	SetWarpTargetYaw(WarpTargetName, Yaw);
}

void UAPMotionWarpingComponent::Server_RemoveWarpTarget_Implementation(FName WarpTargetName)
{
	if (!IsClientSettableWarpTarget(WarpTargetName))
	{
		return;
	}

	RemoveWarpTarget(WarpTargetName);
}

bool UAPMotionWarpingComponent::IsClientSettableWarpTarget(FName WarpTargetName)
{
	//클라이언트가 결정권을 갖는 워프 타깃은 입력 방향 기반 Facing뿐
	return WarpTargetName == WarpTargetName_Facing;
}

bool UAPMotionWarpingComponent::ShouldSendToServer() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn && !OwnerPawn->HasAuthority() && OwnerPawn->IsLocallyControlled();
}
