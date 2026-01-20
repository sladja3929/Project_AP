#include "GAS/Abilities/Tasks/MontageCurvePoller.h"
#include "Animation/AnimInstance.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogMontageCurvePoller, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogMontageCurvePoller, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void FMontageCurvePoller::Initialize(const TArray<FName>& InCurveNames)
{
	CurveNames = InCurveNames;
	ActiveStates.Empty();
	CurrentValues.Empty();
	RisingEdgeCurves.Empty();
	FallingEdgeCurves.Empty();

	//모든 커브를 비활성 상태로 초기화
	for (const FName& CurveName : CurveNames)
	{
		ActiveStates.Add(CurveName, false);
		CurrentValues.Add(CurveName, 0.0f);
	}

	DEBUG_LOG(TEXT("MontageCurvePoller Initialized with %d curves"), CurveNames.Num());
}

void FMontageCurvePoller::Poll(UAnimInstance* AnimInstance)
{
	if (!AnimInstance)
	{
		return;
	}

	//이번 프레임 에지 목록 초기화
	RisingEdgeCurves.Empty();
	FallingEdgeCurves.Empty();

	//GetActiveCurveNames 최적화 제거 - 항상 직접 GetCurveValue 호출
	//추적 커브가 2개뿐이므로 직접 호출이 더 안전하고 에지 누락/오탐 위험 없음
	for (const FName& CurveName : CurveNames)
	{
		float NewValue = AnimInstance->GetCurveValue(CurveName);
		CurrentValues.Add(CurveName, NewValue);
		//DEBUG_LOG(TEXT("Curve Value: %s (Value: %.3f)"), *CurveName.ToString(), NewValue);
		
		//히스테리시스 상태머신
		bool bWasActive = ActiveStates.FindRef(CurveName);
		bool bIsNowActive = bWasActive;

		if (NewValue >= RisingThreshold)
		{
			bIsNowActive = true;
		}
		else if (NewValue <= FallingThreshold)
		{
			bIsNowActive = false;
		}
		//else: 중간 영역에서는 이전 상태 유지

		//에지 감지
		if (!bWasActive && bIsNowActive)
		{
			RisingEdgeCurves.Add(CurveName);
			DEBUG_LOG(TEXT("Rising Edge Detected: %s (Value: %.3f)"), *CurveName.ToString(), NewValue);
		}
		else if (bWasActive && !bIsNowActive)
		{
			FallingEdgeCurves.Add(CurveName);
			DEBUG_LOG(TEXT("Falling Edge Detected: %s (Value: %.3f)"), *CurveName.ToString(), NewValue);
		}

		ActiveStates.Add(CurveName, bIsNowActive);
	}
}

bool FMontageCurvePoller::IsCurveActive(FName CurveName) const
{
	const bool* bIsActive = ActiveStates.Find(CurveName);
	return bIsActive ? *bIsActive : false;
}

bool FMontageCurvePoller::HasRisingEdge(FName CurveName) const
{
	return RisingEdgeCurves.Contains(CurveName);
}

bool FMontageCurvePoller::HasFallingEdge(FName CurveName) const
{
	return FallingEdgeCurves.Contains(CurveName);
}

float FMontageCurvePoller::GetCurveValue(FName CurveName) const
{
	const float* Value = CurrentValues.Find(CurveName);
	return Value ? *Value : 0.0f;
}

void FMontageCurvePoller::Reset()
{
	ActiveStates.Empty();
	CurrentValues.Empty();
	RisingEdgeCurves.Empty();
	FallingEdgeCurves.Empty();

	for (const FName& CurveName : CurveNames)
	{
		ActiveStates.Add(CurveName, false);
		CurrentValues.Add(CurveName, 0.0f);
	}

	DEBUG_LOG(TEXT("MontageCurvePoller Reset"));
}
