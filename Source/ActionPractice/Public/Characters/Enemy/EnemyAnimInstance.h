#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

class AEnemyCharacter;

/**
 * 적 ABP 공통 부모 AnimInstance
 * - 전투 대상(AEnemyCharacter::CombatTarget, 복제)을 향한 머리 바라보기 각도 계산
 *   목·머리 본에 나눠 줄 컴포넌트 공간 Yaw 회전(NeckLookRotation, HeadLookRotation)을 ABP의 Transform (Modify) Bone 노드가 읽음
 *   (피치는 건드리지 않음. 사족 보행은 목 본 축이 위로 들려 있어 Look At으로 축을 맞추면 목이 꺾임)
 * - 몽타주 재생 중(Look At Target ANS 구간 제외)이거나 대상이 MaxLookAngle 밖이면 0으로 보간 복귀
 * - 저속 보폭 보정: 걷기 재생 배율 하한 아래 속도에서 남는 이동량을 StrideScale로 계산해 ABP Stride Warping 노드가 보폭을 줄임
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//목 본에 더할 컴포넌트 공간 회전 (Transform (Modify) Bone: Rotation Mode Add To Existing, Rotation Space Component Space)
	UPROPERTY(BlueprintReadOnly, Category = "Look At")
	FRotator NeckLookRotation = FRotator::ZeroRotator;

	//머리 본에 더할 컴포넌트 공간 회전
	UPROPERTY(BlueprintReadOnly, Category = "Look At")
	FRotator HeadLookRotation = FRotator::ZeroRotator;

	//ABP Stride Warping 노드의 Stride Scale (1이면 보폭 그대로)
	UPROPERTY(BlueprintReadOnly, Category = "Stride Warping")
	float StrideScale = 1.0f;

#pragma endregion

#pragma region "Public Functions"

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//목·머리가 함께 돌 수 있는 최대 수평 각도
	UPROPERTY(EditDefaultsOnly, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float MaxLookYaw = 70.0f;

	//대상이 이 각도(정면 기준) 밖이면 바라보기 해제 (몸 회전은 StateTree 담당)
	UPROPERTY(EditDefaultsOnly, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxLookAngle = 120.0f;

	//해제 후 다시 바라보기 시작하는 각도 여유 (MaxLookAngle - 이 값 안으로 들어와야 재개, 경계 진동 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float LookAngleHysteresis = 15.0f;

	//전체 회전 중 목이 맡는 비율 (나머지는 머리)
	UPROPERTY(EditDefaultsOnly, Category = "Look At", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeckShare = 0.4f;

	//회전 보간 속도
	UPROPERTY(EditDefaultsOnly, Category = "Look At", meta = (ClampMin = "0.0"))
	float LookInterpSpeed = 6.0f;

	//저속 보폭 보정 사용 여부 (ABP에 Stride Warping 노드를 구성한 적만 켬)
	UPROPERTY(EditDefaultsOnly, Category = "Stride Warping")
	bool bEnableStrideWarping = false;

	//걷기 애니메이션 기준 속도 (재생 배율 1에서 디딤발이 지면을 미는 속도). 블렌드스페이스 걷기 샘플 RateScale = 속도 / 이 값
	UPROPERTY(EditDefaultsOnly, Category = "Stride Warping", meta = (ClampMin = "1.0", EditCondition = "bEnableStrideWarping"))
	float StrideReferenceSpeed = 355.0f;

	//걷기 재생 배율 하한. 블렌드스페이스 저속 샘플 RateScale 하한과 같은 값이어야 함
	//이보다 느린 속도는 재생을 더 늦추지 않고 보폭을 줄여 맞춤 (재생만 늦추면 슬로모션처럼 보임)
	UPROPERTY(EditDefaultsOnly, Category = "Stride Warping", meta = (ClampMin = "0.05", ClampMax = "1.0", EditCondition = "bEnableStrideWarping"))
	float MinLocomotionPlayRate = 0.5f;

	//보폭 배율 하한 (과도한 다리 굽힘 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Stride Warping", meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bEnableStrideWarping"))
	float MinStrideScale = 0.4f;

	//이 속도까지는 보정량을 0에서 서서히 올림 (정지·출발 순간 보폭이 튀지 않도록)
	UPROPERTY(EditDefaultsOnly, Category = "Stride Warping", meta = (ClampMin = "1.0", EditCondition = "bEnableStrideWarping"))
	float StrideBlendInSpeed = 50.0f;

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AEnemyCharacter> OwnerEnemy;

	//현재 적용 중인 전체 Yaw (보간 상태)
	float CurrentLookYaw = 0.0f;

	//각도 기준으로 바라보기 중인지 (히스테리시스 상태)
	bool bLookingByAngle = false;

#pragma endregion

#pragma region "Private Functions"

	//이번 프레임 목표 Yaw (바라보기 불가면 0)
	float ComputeTargetLookYaw();

	//Look At Target ANS 구간이 활성인지 (직전 노티파이 처리 결과 기준)
	bool IsLookAtNotifyActive() const;

	//현재 수평 속도로 StrideScale 계산
	void UpdateStrideScale();

#pragma endregion
};
