#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AActor;

struct FLockOnView
{
	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;
};

struct FLockOnCandidate
{
	TWeakObjectPtr<AActor> Actor;
	FVector AimPoint = FVector::ZeroVector;
	float Score = 0.0f;
};

//점수 파라미터
struct FLockOnScoringParams
{
	float MaxDistance = 2000.0f;
	float MaxAngleDegrees = 60.0f;
	float AngleWeight = 0.7f;
	float DistanceWeight = 0.3f;
};

//락온 타겟 선택의 순수 계산부
//월드, 액터, 물리에 접근하지 않으므로 Automation Test로 단독 검증 가능
class ACTIONPRACTICE_API FLockOnTargetSelector
{
public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	//카메라 평면(정규화 투영 평면)으로 월드 좌표를 투영
	//카메라 뒤(또는 정확히 옆)이면 false
	static bool ProjectToCameraPlane(const FLockOnView& View, const FVector& Point, FVector2D& OutPos);

	//탈락 조건을 검사하고 통과하면 Score를 채움. 탈락 시 false
	static bool ScoreCandidate(const FLockOnView& View, const FVector& OwnerLocation, const FLockOnScoringParams& Params, FLockOnCandidate& InOutCandidate);

	//Score 기준 최대 힙 구성
	static void BuildRankedHeap(TArray<FLockOnCandidate>& InOutCandidates);
	
	static bool PopRankedCandidate(TArray<FLockOnCandidate>& InOutHeap, FLockOnCandidate& OutCandidate);

	//좌우(화면 평면) 방향 전환 대상 index를 선택
	static int32 SelectSwitchTarget(const FLockOnView& View, const TArray<FLockOnCandidate>& Candidates, const FVector& CurrentAimPoint, const FVector2D& InputDir, float DistancePenaltyWeight);

	static bool IsHigherScore(const FLockOnCandidate& A, const FLockOnCandidate& B);

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
