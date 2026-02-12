#pragma once

#include "CoreMinimal.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "ActionPracticeAbilitySystemComponent.generated.h"

class AActionPracticeCharacter;
class UActionPracticeAttributeSet;

USTRUCT()
struct FGameplayEventData_NetPredicted
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag EventTag;

	UPROPERTY()
	FGameplayTagContainer InstigatorTags;

	UPROPERTY()
	float EventMagnitude = 0.0f;
};

UCLASS()
class ACTIONPRACTICE_API UActionPracticeAbilitySystemComponent : public UBaseAbilitySystemComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	UActionPracticeAbilitySystemComponent();

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	UFUNCTION(BlueprintPure, Category="Attributes")
	const UActionPracticeAttributeSet* GetActionPracticeAttributeSet() const;

	//WaitInputRelease / WaitInputPress 계열 태스크용 입력 이벤트 브릿지
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	//HandleGameplayEvent 계열 태스크용 이벤트 브릿지
	void HandleGameplayEvent_NetPredicted(FGameplayTag EventTag, const FGameplayEventData* Payload);

	// ===== 태그 관리 =====
	// 서버: AuthTag를 MinimalReplication으로 추가, 클라: LocalTag를 LooseTag로 추가
	void AddTag_NetPredicted(FGameplayTag AuthTag, FGameplayTag LocalTag);

	// 서버: AuthTag 전부 제거, 클라: LocalTag 전부 제거
	void RemoveTags_NetPredicted(FGameplayTag AuthTag, FGameplayTag LocalTag);

	//===== Defense Policy Override =====
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;
	virtual void PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TObjectPtr<AActionPracticeCharacter> OwnerCharacter;
	FGameplayTag EffectStaminaRegenBlockDurationTag;
	FGameplayTag StateAbilityBlockingTag;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
#pragma region "Private Variables"

	FActiveGameplayEffectHandle StaminaRegenBlockHandle;

	//블로킹 관련 변수
	bool bBlockedLastAttack = false;

#pragma endregion

#pragma region "Private Functions"

	//HandleGameplayEvent 계열 태스크용 RPC
	UFUNCTION(Server, Reliable)
	void Server_HandleGameplayEvent(const FGameplayEventData_NetPredicted& Payload);
	
	void CheckBlockSuccess(AActor* SourceActor);

#pragma endregion
};
