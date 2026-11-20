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

#pragma endregion
};
