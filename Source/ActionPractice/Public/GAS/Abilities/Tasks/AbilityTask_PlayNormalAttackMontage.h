#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityTask_PlayNormalAttackMontage.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNormalAttackMontageDelegate);

/***
* 레거시 클래스: 사용하지 않음, PlayMontageWithEvents로 대체
*/

UCLASS()
class ACTIONPRACTICE_API UAbilityTask_PlayNormalAttackMontage : public UAbilityTask
{
    GENERATED_BODY()

public:
#pragma region "Public Variables" //=======================================================
    
    // 델리게이트들
    UPROPERTY(BlueprintAssignable)
    FNormalAttackMontageDelegate OnCompleted;

    UPROPERTY(BlueprintAssignable)
    FNormalAttackMontageDelegate OnBlendOut;

    UPROPERTY(BlueprintAssignable)
    FNormalAttackMontageDelegate OnInterrupted;

    UPROPERTY(BlueprintAssignable)
    FNormalAttackMontageDelegate OnCancelled;

    // 콤보 진행시 호출
    UPROPERTY(BlueprintAssignable)
    FNormalAttackMontageDelegate OnComboPerformed;
    
    UPROPERTY()
    int32 ComboCounter = 0;

    UPROPERTY()
    int32 MaxComboCount = 3;

    UPROPERTY()
    bool bCanComboSave = false;

    UPROPERTY()
    bool bComboInputSaved = false;

    UPROPERTY()
    bool bIsInCancellableRecovery = false;

    // 콤보 전환 중인지 표시하는 플래그
    UPROPERTY()
    bool bIsTransitioningToNextCombo = false;
    
    // 어빌리티가 취소되면 몽타주 정지
    UPROPERTY()
    bool bStopMontageWhenAbilityCancelled;
    
#pragma endregion

#pragma region "Public Functions" //==========================================================

    UAbilityTask_PlayNormalAttackMontage(const FObjectInitializer& ObjectInitializer);

    // 태스크 생성 함수
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "PlayNormalAttackMontage",
        HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
    static UAbilityTask_PlayNormalAttackMontage* CreatePlayNormalAttackMontageProxy(
        UGameplayAbility* OwningAbility,
        FName TaskInstanceName,
        const TArray<TSoftObjectPtr<UAnimMontage>>& MontagesToPlay,
        float Rate = 1.0f,
        FName StartSection = NAME_None,
        float AnimRootMotionTranslationScale = 1.0f);

    // 태스크 활성화
    virtual void Activate() override;

    // 태스크 정리
    virtual void OnDestroy(bool AbilityEnded) override;

    // 외부 정지
    virtual void ExternalCancel() override;

    // 콤보 입력 처리 (원본의 InputPressed 로직)
    void CheckComboInputPreseed();

    // 다음 콤보 몽타주 재생
    void PlayNextAttackCombo();
    
#pragma endregion

protected:
#pragma region "Protected Variables" //=============================================================

    // 몽타주 배열 (콤보별)
    UPROPERTY()
    TArray<TSoftObjectPtr<UAnimMontage>> MontagesToPlay;
    
    // 현재 재생 중인 몽타주
    UPROPERTY()
    UAnimMontage* CurrentMontage;

    // 재생 속도
    UPROPERTY()
    float Rate;

    // 시작 섹션
    UPROPERTY()
    FName StartSectionName;

    // 루트 모션 스케일
    UPROPERTY()
    float AnimRootMotionTranslationScale;

    // 몽타주 델리게이트 핸들
    FOnMontageBlendingOutStarted BlendingOutDelegate;
    FOnMontageEnded MontageEndedDelegate;

    // 이벤트 핸들
    FDelegateHandle EnableBufferInputHandle;
    FDelegateHandle ActionRecoveryEndHandle;
    FDelegateHandle ResetComboHandle;
    
#pragma endregion
    
#pragma region "Protected Functions" //=============================================================
    
    UFUNCTION()
    void PlayAttackMontage();
    
    UFUNCTION()
    void StopPlayingMontage();

    // 이벤트 핸들러
    UFUNCTION()
    void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    
    UFUNCTION()
    void HandleEnableBufferInputEvent(const FGameplayEventData& Payload);

    UFUNCTION()
    void HandleActionRecoveryEndEvent(const FGameplayEventData& Payload);

    UFUNCTION()
    void HandleResetComboEvent(const FGameplayEventData& Payload);

    // 이벤트 핸들 등록/해제
    void RegisterGameplayEventCallbacks();
    void UnregisterGameplayEventCallbacks();
    
#pragma endregion
};