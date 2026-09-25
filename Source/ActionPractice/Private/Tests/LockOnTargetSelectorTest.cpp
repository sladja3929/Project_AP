#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Characters/LockOn/LockOnTargetSelector.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogLockOnTargetSelectorTest, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogLockOnTargetSelectorTest, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace LockOnTargetSelectorTestHelper
{
	//원점에서 +X를 바라보는 표준 뷰. 월드나 액터를 만들지 않는다
	static FLockOnView MakeDefaultView()
	{
		FLockOnView View;
		View.Location = FVector::ZeroVector;
		View.Forward = FVector::ForwardVector;
		View.Right = FVector::RightVector;
		View.Up = FVector::UpVector;

		return View;
	}

	static FLockOnScoringParams MakeDefaultParams()
	{
		FLockOnScoringParams Params;
		Params.MaxDistance = 1000.0f;
		Params.MaxAngleDegrees = 60.0f;
		Params.AngleWeight = 0.7f;
		Params.DistanceWeight = 0.3f;

		return Params;
	}

	static FLockOnCandidate MakeCandidate(const FVector& AimPoint, float Score = 0.0f)
	{
		FLockOnCandidate Candidate;
		Candidate.AimPoint = AimPoint;
		Candidate.Score = Score;

		return Candidate;
	}
}

#pragma region "ProjectToCameraPlane"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockOnProjectToCameraPlaneTest, "ActionPractice.LockOn.ProjectToCameraPlane", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLockOnProjectToCameraPlaneTest::RunTest(const FString& Parameters)
{
	const FLockOnView View = LockOnTargetSelectorTestHelper::MakeDefaultView();

	//카메라 앞
	FVector2D Projected = FVector2D::ZeroVector;
	TestTrue(TEXT("Front point projects"), FLockOnTargetSelector::ProjectToCameraPlane(View, FVector(100.0f, 50.0f, 25.0f), Projected));
	TestEqual(TEXT("Front X"), Projected.X, 0.5, 1e-4);
	TestEqual(TEXT("Front Y"), Projected.Y, 0.25, 1e-4);

	//카메라 뒤
	TestFalse(TEXT("Behind point fails"), FLockOnTargetSelector::ProjectToCameraPlane(View, FVector(-100.0f, 0.0f, 0.0f), Projected));

	//정확히 옆 (깊이 0)
	TestFalse(TEXT("Side point fails"), FLockOnTargetSelector::ProjectToCameraPlane(View, FVector(0.0f, 100.0f, 0.0f), Projected));

	return true;
}

#pragma endregion

#pragma region "ScoreCandidate"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockOnScoreCandidateTest, "ActionPractice.LockOn.ScoreCandidate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLockOnScoreCandidateTest::RunTest(const FString& Parameters)
{
	const FLockOnView View = LockOnTargetSelectorTestHelper::MakeDefaultView();
	const FLockOnScoringParams Params = LockOnTargetSelectorTestHelper::MakeDefaultParams();
	const FVector OwnerLocation = FVector::ZeroVector;

	//카메라 뒤 후보는 탈락
	FLockOnCandidate Behind = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(-500.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Behind candidate rejected"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, Behind));

	//최대 거리 안쪽은 통과
	FLockOnCandidate InRange = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(999.0f, 0.0f, 0.0f));
	TestTrue(TEXT("In-range candidate accepted"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, InRange));

	//최대 거리 바깥은 탈락
	FLockOnCandidate OutOfRange = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1001.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Out-of-range candidate rejected"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, OutOfRange));

	//최대 각도 안쪽은 통과
	const float InAngleRadians = FMath::DegreesToRadians(59.0f);
	FLockOnCandidate InAngle = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(500.0f * FMath::Cos(InAngleRadians), 500.0f * FMath::Sin(InAngleRadians), 0.0f));
	TestTrue(TEXT("In-angle candidate accepted"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, InAngle));

	//최대 각도 바깥은 탈락
	const float OutAngleRadians = FMath::DegreesToRadians(61.0f);
	FLockOnCandidate OutAngle = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(500.0f * FMath::Cos(OutAngleRadians), 500.0f * FMath::Sin(OutAngleRadians), 0.0f));
	TestFalse(TEXT("Out-of-angle candidate rejected"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, OutAngle));

	//같은 거리에서 화면 중앙 후보가 구석 후보보다 높은 점수
	const float CornerRadians = FMath::DegreesToRadians(45.0f);
	FLockOnCandidate Center = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(500.0f, 0.0f, 0.0f));
	FLockOnCandidate Corner = LockOnTargetSelectorTestHelper::MakeCandidate(FVector(500.0f * FMath::Cos(CornerRadians), 500.0f * FMath::Sin(CornerRadians), 0.0f));

	TestTrue(TEXT("Center candidate accepted"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, Center));
	TestTrue(TEXT("Corner candidate accepted"), FLockOnTargetSelector::ScoreCandidate(View, OwnerLocation, Params, Corner));
	TestTrue(TEXT("Center scores higher than corner"), Center.Score > Corner.Score);

	return true;
}

#pragma endregion

#pragma region "BuildRankedHeap"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockOnBuildRankedHeapTest, "ActionPractice.LockOn.BuildRankedHeap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLockOnBuildRankedHeapTest::RunTest(const FString& Parameters)
{
	//결정적 재현을 위해 고정 시드 사용
	FRandomStream RandomStream(20260921);

	TArray<FLockOnCandidate> Candidates;
	Candidates.Reserve(64);

	for (int32 Index = 0; Index < 64; ++Index)
	{
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector::ZeroVector, RandomStream.FRandRange(0.0f, 1.0f)));
	}

	FLockOnTargetSelector::BuildRankedHeap(Candidates);

	float PreviousScore = TNumericLimits<float>::Max();
	int32 PoppedCount = 0;
	FLockOnCandidate Popped;

	while (FLockOnTargetSelector::PopRankedCandidate(Candidates, Popped))
	{
		TestTrue(TEXT("HeapPop order is descending"), Popped.Score <= PreviousScore);
		PreviousScore = Popped.Score;
		++PoppedCount;
	}

	TestEqual(TEXT("All candidates popped"), PoppedCount, 64);

	return true;
}

#pragma endregion

#pragma region "SelectSwitchTarget"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockOnSelectSwitchTargetTest, "ActionPractice.LockOn.SelectSwitchTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLockOnSelectSwitchTargetTest::RunTest(const FString& Parameters)
{
	const FLockOnView View = LockOnTargetSelectorTestHelper::MakeDefaultView();
	const FVector CurrentAimPoint(1000.0f, 0.0f, 0.0f);
	const FVector2D RightInput(1.0f, 0.0f);
	const float PenaltyWeight = 0.5f;

	//오른쪽 입력에 오른쪽 후보 선택. 수직(내적 0) 후보와 왼쪽 후보는 제외
	{
		TArray<FLockOnCandidate> Candidates;
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, -200.0f, 0.0f)));
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 0.0f, 200.0f)));
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 200.0f, 0.0f)));

		const int32 Selected = FLockOnTargetSelector::SelectSwitchTarget(View, Candidates, CurrentAimPoint, RightInput, PenaltyWeight);
		TestEqual(TEXT("Right input selects right candidate"), Selected, 2);
	}

	//입력 방향과 정확히 수직인 후보만 있으면 선택하지 않는다
	{
		TArray<FLockOnCandidate> Candidates;
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 0.0f, 200.0f)));
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 0.0f, -200.0f)));

		const int32 Selected = FLockOnTargetSelector::SelectSwitchTarget(View, Candidates, CurrentAimPoint, RightInput, PenaltyWeight);
		TestEqual(TEXT("Perpendicular candidates are excluded"), Selected, static_cast<int32>(INDEX_NONE));
	}

	//해당 방향에 후보가 없으면 INDEX_NONE
	{
		TArray<FLockOnCandidate> Candidates;
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, -200.0f, 0.0f)));
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, -400.0f, 0.0f)));

		const int32 Selected = FLockOnTargetSelector::SelectSwitchTarget(View, Candidates, CurrentAimPoint, RightInput, PenaltyWeight);
		TestEqual(TEXT("No candidate in input direction"), Selected, static_cast<int32>(INDEX_NONE));
	}

	//동점 후보는 먼저 나온 인덱스를 유지한다
	{
		TArray<FLockOnCandidate> Candidates;
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 200.0f, 200.0f)));
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 200.0f, -200.0f)));

		const int32 Selected = FLockOnTargetSelector::SelectSwitchTarget(View, Candidates, CurrentAimPoint, RightInput, PenaltyWeight);
		TestEqual(TEXT("Tie keeps the first index"), Selected, 0);
	}

	//빈 입력 방향은 선택하지 않는다
	{
		TArray<FLockOnCandidate> Candidates;
		Candidates.Add(LockOnTargetSelectorTestHelper::MakeCandidate(FVector(1000.0f, 200.0f, 0.0f)));

		const int32 Selected = FLockOnTargetSelector::SelectSwitchTarget(View, Candidates, CurrentAimPoint, FVector2D::ZeroVector, PenaltyWeight);
		TestEqual(TEXT("Zero input direction selects nothing"), Selected, static_cast<int32>(INDEX_NONE));
	}

	return true;
}

#pragma endregion

#endif //WITH_DEV_AUTOMATION_TESTS
