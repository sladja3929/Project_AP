#pragma once

#include "CoreMinimal.h"
#include "MontageCurvePoller.generated.h"

class UAnimInstance;

//몽타주 커브 폴링을 위한 구조체
//히스테리시스 상태머신으로 블렌딩 중에도 안정적인 에지 감지
USTRUCT()
struct ACTIONPRACTICE_API FMontageCurvePoller
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//폴링할 커브 이름 목록
	UPROPERTY()
	TArray<FName> CurveNames;

	//히스테리시스 상태머신 - 이전 활성 상태를 별도 기억
	//TMap<FName, bool>은 리플렉션에서 bool 지원 제한으로 UPROPERTY 불가
	TMap<FName, bool> ActiveStates;

	//현재 커브 값들
	TMap<FName, float> CurrentValues;

	//이번 프레임에서 상승 에지가 발생한 커브들
	TArray<FName> RisingEdgeCurves;

	//이번 프레임에서 하강 에지가 발생한 커브들
	TArray<FName> FallingEdgeCurves;

	//히스테리시스 임계값 (블렌딩 대응)
	float RisingThreshold = 0.6f;

	float FallingThreshold = 0.5f;

#pragma endregion

#pragma region "Public Functions"

	//커브 폴러 초기화
	void Initialize(const TArray<FName>& InCurveNames);

	//커브 추가
	void AddCurve(const FName InCurveName);
	
	//폴링 실행 - TickTask에서 호출
	void Poll(UAnimInstance* AnimInstance);

	//커브가 현재 활성 상태인지 확인
	bool IsCurveActive(FName CurveName) const;

	//이번 프레임에서 상승 에지가 발생했는지 확인
	bool HasRisingEdge(FName CurveName) const;

	//이번 프레임에서 하강 에지가 발생했는지 확인
	bool HasFallingEdge(FName CurveName) const;

	//현재 커브 값 가져오기
	float GetCurveValue(FName CurveName) const;

	//상태 초기화
	void Reset();

#pragma endregion
};
