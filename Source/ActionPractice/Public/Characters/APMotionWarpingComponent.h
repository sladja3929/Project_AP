#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingComponent.h"
#include "APMotionWarpingComponent.generated.h"

/***
 * 몽타주 기반 회전/이동 보정용 모션 워핑 컴포넌트
 * - 워프 타깃 이름을 프로젝트 공용 상수로 관리 (몽타주 MotionWarping ANS의 Warp Target Name과 일치해야 함)
 * - 서버 → 시뮬레이티드 프록시 복제는 엔진이 처리 (WarpTargets, COND_SimulatedOnly)
 * - 오너 클라이언트만 아는 정보(입력 방향 등)는 엔진이 서버로 올려주지 않으므로 Predicted 함수로 로컬 적용 + Server RPC 전달
 */
UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UAPMotionWarpingComponent : public UMotionWarpingComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//지정 Yaw로 회전하는 워프 타깃 (ANS 설정: Rotation Type = Default, Warp Translation off)
	static const FName WarpTargetName_Facing;

	//타깃 액터를 바라보며 추적하는 워프 타깃 (ANS 설정: Rotation Type = Facing, Warp Translation off)
	static const FName WarpTargetName_AttackTarget;

	//타깃 앞 정지 지점까지 이동시키는 워프 타깃 (ANS 설정: Warp Translation on, Warp Rotation off)
	//회전은 같은 구간의 AttackTarget ANS가 담당 (정지 지점을 바라보면 도착 직전 방향이 불안정)
	//애니메이션 이동량이 0인 구간은 SkewWarp가 AddTranslationEasingFunc 곡선으로 이동량을 생성
	static const FName WarpTargetName_AttackApproach;

	//제자리 회전 몽타주가 타깃을 바라볼 때까지 회전하는 워프 타깃 (ANS 설정: Rotation Type = Facing, Rotation Method = Scale, Warp Translation off)
	//Scale은 애니메이션 자체의 루트 회전량을 실제 필요한 각도에 맞춰 늘리거나 줄임 (회전 모션이 있는 시퀀스 전용)
	//Scale은 프레임 회전량을 ±WarpMaxRotationRate*DeltaSeconds로 클램프하므로 WarpMaxRotationRate 0이면 회전하지 않음 (충분히 큰 값 필요)
	static const FName WarpTargetName_TurnTarget;

#pragma endregion

#pragma region "Public Functions"

	UAPMotionWarpingComponent(const FObjectInitializer& ObjectInitializer);

	//지정 Yaw를 목표 회전으로 하는 워프 타깃 설정 (로컬 전용)
	//서버/클라 양쪽이 같은 값을 계산할 수 있는 경우(ServerInitiated 어빌리티 등) 각자 호출
	void SetWarpTargetYaw(FName WarpTargetName, float Yaw);

	//오너 클라이언트에서 계산한 Yaw를 로컬 적용 + 원격 클라이언트일 경우 서버에 전달
	void SetWarpTargetYawPredicted(FName WarpTargetName, float Yaw);

	//오너 클라이언트에서 워프 타깃 제거 + 원격 클라이언트일 경우 서버에 전달
	void RemoveWarpTargetPredicted(FName WarpTargetName);

	//타깃 액터의 루트 컴포넌트를 추적하는 워프 타깃 설정 (서버에서 호출 시 시뮬 프록시에 자동 복제)
	//StopDistance > 0이면 타깃에서 오너 쪽으로 StopDistance만큼 떨어진 지점을 추적 (이동 워프용)
	//타깃이 없으면 워프 타깃 제거 → 해당 워프 윈도우 비활성
	void SetWarpTargetFollowActor(FName WarpTargetName, const AActor* TargetActor, float StopDistance = 0.0f);

#pragma endregion

private:
#pragma region "Private Functions"

	UFUNCTION(Server, Reliable)
	void Server_SetWarpTargetYaw(FName WarpTargetName, float Yaw);

	UFUNCTION(Server, Reliable)
	void Server_RemoveWarpTarget(FName WarpTargetName);

	//원격 클라이언트(서버 권한 없음 + 로컬 컨트롤)에서만 RPC 필요
	bool ShouldSendToServer() const;

	//Server RPC로 클라이언트가 설정할 수 있는 워프 타깃인지 여부
	static bool IsClientSettableWarpTarget(FName WarpTargetName);

#pragma endregion
};
