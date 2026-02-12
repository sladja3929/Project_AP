#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Input/InputActionDataAsset.h"
#include "InputBufferComponent.generated.h"

class AActionPracticeCharacter;
class UInputAction;
class UActionPracticeAbility;
class UGameplayAbility;
class UInputActionDataAsset;
struct FGameplayEventData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//버퍼 오픈 여부, Character의 입력 저장 진입에 사용
    UPROPERTY(BlueprintReadOnly, Category="Input Buffer")
    bool bBufferWindowOpened = false; 

    //한 번에 하나만 버퍼되는 단발 액션 (Both + Release 포함)
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    FGameplayTag BufferedActionTag;

    //BufferedActionTag의 우선순위
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    int32 BufferPriority = -1;

    //BufferedActionTag가 Release 이벤트인지 여부
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    bool bBufferedActionReleased = false;
    
    //동시에 여러 개 버퍼될 수 있는 홀드 액션들
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    TSet<FGameplayTag> BufferedHoldActionTags;

#pragma endregion

#pragma region "Public Functions"

    UInputBufferComponent();

    //서버/클라 분류용, 외부에서는 이 함수만 호출
    UFUNCTION()
	void BufferInput(const UInputAction* InputAction, bool bIsReleased);

    UFUNCTION()
    bool IsBufferWaiting();

    //버퍼 실행
    void ExecuteBuffer();

    void EnableBufferInput(bool bEnabled);

#pragma endregion

protected:
#pragma region "Protected Variables"

    //버퍼 저장 시점의 상태 태그 (Roll, Sprint 등 판정용)
    UPROPERTY()
    FGameplayTagContainer BufferedStateTags;

    FDelegateHandle EnableBufferInputHandle;
    FDelegateHandle PlayBufferHandle;

    FGameplayTag EventNotifyEnableBufferInputTag;
    FGameplayTag EventActionInputByBufferTag;
    FGameplayTag EventActionPlayBufferTag;

#pragma endregion

#pragma region "Protected Functions"

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BufferInputInternal(FGameplayTag InputActionTag, bool bIsReleased);

    void ActivateAbilityByTag(FGameplayTag ActionTag);

    void CaptureCurrentStateTags();

#pragma endregion

private:
#pragma region "Private Variables"

    UPROPERTY()
    TObjectPtr<AActionPracticeCharacter> OwnerCharacter = nullptr;

    UPROPERTY()
    TObjectPtr<const UInputActionDataAsset> CachedInputActionData = nullptr;

#pragma endregion

#pragma region "Private Functions"

    bool CheckActionRule(FGameplayTag ActionTag, int32& OutPriority, EInputBehavior& OutInputBehavior) const;

    void ActivateAbility(const UInputAction* InputAction);

#pragma endregion
};