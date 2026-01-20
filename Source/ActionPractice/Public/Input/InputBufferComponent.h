#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "InputBufferComponent.generated.h"

class AActionPracticeCharacter;
class UInputAction;
class UActionPracticeAbility;
class UGameplayAbility;
class UInputActionDataAsset;
struct FGameplayEventData;

//대기 입력 구조체 - TTL/그레이스용
USTRUCT()
struct FPendingInput
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag ActionTag;

	UPROPERTY()
	bool bIsReleased = false;

	UPROPERTY()
	float Timestamp = 0.0f;

	UPROPERTY()
	bool bIsHoldAction = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//클라이언트의 버퍼 오픈 여부, Character의 입력 저장 진입에 사용
    UPROPERTY(BlueprintReadOnly, Category="Input Buffer")
    bool bInternalBufferEnabled = false; 

    //이번 버퍼 입력이 Release인지 여부
    UPROPERTY(BlueprintReadOnly, Category="Input Buffer")
    bool bBufferActionReleased = false;

    //한 번에 하나만 버퍼되는 단발 액션
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    FGameplayTag BufferedActionTag;

    //동시에 여러 개 버퍼될 수 있는 홀드 액션들
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    TSet<FGameplayTag> BufferedHoldActionTags;

    //BufferedActionTag의 우선순위
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    int32 BufferPriority = -1;

    //BufferedActionTag가 Release 이벤트인지 여부
    UPROPERTY(VisibleInstanceOnly, Category="Input Buffer")
    bool bBufferedActionReleased = false;

    // ==== TTL / 그레이스 ====

    //서버 버퍼가 닫혀 있을 때 들어온 입력을 잠시 보관
    UPROPERTY()
    TMap<FGameplayTag, FPendingInput> PendingInputs;

    //PendingInputs가 살아있는 최대 시간(초) - 기본 0.1초 = 100ms.
    UPROPERTY(EditDefaultsOnly, Category="Input Buffer")
    float InputGracePeriod = 0.1f;

#pragma endregion

#pragma region "Public Functions"

    UInputBufferComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    //서버/클라 분류용, 외부에서는 이 함수만 호출
    UFUNCTION()
	void BufferInput(const UInputAction* InputAction, bool bIsReleased);

    UFUNCTION()
    bool IsBufferWaiting();

    //내부 공통 실행 함수
    void ExecuteBuffer();

    //버퍼 윈도우 변경
    void EnableBufferInput(bool bEnabled);

	//서버 환경에서 TTL + 그레이스 처리
    void ProcessPendingInputs();

#pragma endregion

protected:
#pragma region "Protected Variables"

	//서버의 버퍼 오픈 여부, 권한이 있어야 변경 가능
    UPROPERTY(ReplicatedUsing=OnRepBufferState)
    bool bServerBufferEnabled = false; 

    FDelegateHandle EnableBufferInputHandle;
    FDelegateHandle PlayBufferHandle;

    FGameplayTag EventNotifyEnableBufferInputTag;
    FGameplayTag EventActionInputByBufferTag;
    FGameplayTag EventActionPlayBufferTag;

#pragma endregion

#pragma region "Protected Functions"

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//서버/스탠드어론에서 버퍼 저장
	void InternalBufferInput(FGameplayTag InputActionTag, bool bIsReleased);

	//서버 RPC: 클라이언트에서 서버로 버퍼 저장 요청
	UFUNCTION(Server, Reliable)
	void ServerBufferInput(FGameplayTag InputActionTag, bool bIsReleased);
	
    UFUNCTION()
    void OnRepBufferState();
	
    void ActivateAbilityByTag(FGameplayTag ActionTag);

	//이벤트 핸들러
	void OnEventEnableBufferInput(const FGameplayEventData* Payload);
	void OnEventPlayBuffer(const FGameplayEventData* Payload);

#pragma endregion

private:
#pragma region "Private Variables"

    UPROPERTY()
    TObjectPtr<AActionPracticeCharacter> OwnerCharacter = nullptr;

    UPROPERTY()
    TObjectPtr<const UInputActionDataAsset> CachedInputActionData = nullptr;

#pragma endregion

#pragma region "Private Functions"

    bool CheckActionRule(FGameplayTag ActionTag, int32& OutPriority, bool& bIsHoldAction) const;

    void ActivateAbility(const UInputAction* InputAction);
	
    void CleanupExpiredPendingInputs();

#pragma endregion
};