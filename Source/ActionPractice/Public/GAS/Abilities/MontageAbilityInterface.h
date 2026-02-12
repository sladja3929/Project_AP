#pragma once

#include "UObject/Interface.h"
#include "MontageAbilityInterface.generated.h"

UINTERFACE()
class UMontageAbilityInterface : public UInterface
{
	GENERATED_BODY()
};

class ACTIONPRACTICE_API IMontageAbilityInterface
{
	GENERATED_BODY()
	
public:
	//태스크가 재생할 몽타주 지정
	virtual UAnimMontage* SetMontageToPlayTask() = 0;

	//몽타주 태스크 설정
	virtual void SetUpPlayMontageWithEventsTask() = 0;

	//몽타주 태스크 생성 및 실행
	virtual void StartMontageWithEventsTask() = 0;

	//몽타주 델리게이트 콜백 함수
	virtual void OnTaskMontageCompleted() = 0;
	virtual void OnTaskMontageInterrupted() = 0;
};