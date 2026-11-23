#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_LookAtTarget.generated.h"

/**
 * 몽타주 재생 중 머리 바라보기를 허용하는 구간 표시용 ANS (로직 없음)
 * - UEnemyAnimInstance가 몽타주 재생 중이면 활성 ANS 목록(ActiveAnimNotifyState)에 이 클래스가 있는지 조회해 바라보기 여부 판단
 * - Begin/End 카운터를 두지 않으므로 몽타주 중단·블렌드 아웃으로 End가 누락돼도 상태가 어긋나지 않음
 * - 몸 회전 워프 대신 제자리 대기 몽타주(Combat Idle 등)에서 시선만 타깃을 따라가게 할 때 사용
 */
UCLASS(meta = (DisplayName = "Look At Target"))
class ACTIONPRACTICE_API UAnimNotifyState_LookAtTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Look At Target");
	}

#pragma endregion
};
