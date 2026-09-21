#include "Characters/LockOn/LockOnTargetSelector.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogLockOnTargetSelector, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogLockOnTargetSelector, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#pragma region "Projection"

bool FLockOnTargetSelector::ProjectToCameraPlane(const FLockOnView& View, const FVector& Point, FVector2D& OutPos)
{
	const FVector ToPoint = Point - View.Location;

	const double Depth = FVector::DotProduct(ToPoint, View.Forward);

	//카메라 뒤 또는 정확히 옆(깊이 0)은 대상 X
	if (Depth <= UE_KINDA_SMALL_NUMBER)
	{
		OutPos = FVector2D::ZeroVector;
		return false;
	}

	const double X = FVector::DotProduct(ToPoint, View.Right);
	const double Y = FVector::DotProduct(ToPoint, View.Up);

	OutPos = FVector2D(X / Depth, Y / Depth);
	return true;
}

#pragma endregion

#pragma region "Scoring"

bool FLockOnTargetSelector::ScoreCandidate(const FLockOnView& View, const FVector& OwnerLocation, const FLockOnScoringParams& Params, FLockOnCandidate& InOutCandidate)
{
	InOutCandidate.Score = 0.0f;

	//탈락 1. 카메라 뒤
	FVector2D ProjectedPos = FVector2D::ZeroVector;

	if (!ProjectToCameraPlane(View, InOutCandidate.AimPoint, ProjectedPos))
	{
		return false;
	}

	//탈락 2. 오너 기준 최대 거리 초과
	const float Distance = static_cast<float>(FVector::Dist(OwnerLocation, InOutCandidate.AimPoint));

	if (Params.MaxDistance <= 0.0f || Distance > Params.MaxDistance)
	{
		return false;
	}

	//탈락 3. 카메라 전방 각도 초과
	const FVector ToAim = (InOutCandidate.AimPoint - View.Location).GetSafeNormal();

	if (ToAim.IsNearlyZero())
	{
		return false;
	}

	const double CosAngle = FMath::Clamp(FVector::DotProduct(ToAim, View.Forward.GetSafeNormal()), -1.0, 1.0);
	const float AngleDegrees = FMath::RadiansToDegrees(static_cast<float>(FMath::Acos(CosAngle)));

	if (Params.MaxAngleDegrees <= 0.0f || AngleDegrees > Params.MaxAngleDegrees)
	{
		return false;
	}

	//선호 점수. 각 항 [0,1] 정규화, 높을수록 좋음
	const float AngleScore = 1.0f - (AngleDegrees / Params.MaxAngleDegrees);
	const float DistanceScore = 1.0f - (Distance / Params.MaxDistance);

	InOutCandidate.Score = Params.AngleWeight * AngleScore + Params.DistanceWeight * DistanceScore;

	return true;
}

#pragma endregion

#pragma region "Ranking"

bool FLockOnTargetSelector::IsHigherScore(const FLockOnCandidate& A, const FLockOnCandidate& B)
{
	return A.Score > B.Score;
}

void FLockOnTargetSelector::BuildRankedHeap(TArray<FLockOnCandidate>& InOutCandidates)
{
	//UE Heap 기본은 작은 쪽이 top이므로 > 비교를 넘겨 최고점을 top으로
	InOutCandidates.Heapify(&FLockOnTargetSelector::IsHigherScore);
}

bool FLockOnTargetSelector::PopRankedCandidate(TArray<FLockOnCandidate>& InOutHeap, FLockOnCandidate& OutCandidate)
{
	if (InOutHeap.Num() <= 0)
	{
		return false;
	}

	InOutHeap.HeapPop(OutCandidate, &FLockOnTargetSelector::IsHigherScore, EAllowShrinking::No);
	return true;
}

#pragma endregion

#pragma region "Switching"

int32 FLockOnTargetSelector::SelectSwitchTarget(const FLockOnView& View, const TArray<FLockOnCandidate>& Candidates, const FVector& CurrentAimPoint, const FVector2D& InputDir, float DistancePenaltyWeight)
{
	const FVector2D Direction = InputDir.GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		return INDEX_NONE;
	}

	FVector2D Origin = FVector2D::ZeroVector;

	if (!ProjectToCameraPlane(View, CurrentAimPoint, Origin))
	{
		return INDEX_NONE;
	}

	//탈락이 없는 단일 최적값 문제이므로 힙/정렬 없이 한 번 훑는 argmin O(k)
	int32 BestIndex = INDEX_NONE;
	float BestCost = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		FVector2D Projected = FVector2D::ZeroVector;

		if (!ProjectToCameraPlane(View, Candidates[Index].AimPoint, Projected))
		{
			continue;
		}

		const FVector2D Delta = Projected - Origin;
		const FVector2D DeltaDir = Delta.GetSafeNormal();

		if (DeltaDir.IsNearlyZero())
		{
			continue;
		}

		//반평면 필터. 입력 방향과 수직(내적 0) 이하는 제외
		const double DirDot = FVector2D::DotProduct(DeltaDir, Direction);

		if (DirDot <= 0.0)
		{
			continue;
		}

		const float AngleCost = FMath::Acos(static_cast<float>(FMath::Clamp(DirDot, -1.0, 1.0)));
		const float Cost = AngleCost + DistancePenaltyWeight * static_cast<float>(Delta.Size());

		//동점이면 먼저 나온 인덱스를 유지
		if (Cost < BestCost)
		{
			BestCost = Cost;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

#pragma endregion
