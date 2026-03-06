#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/Player/ActionRecoveryAbility.h"
#include "UseItemAbility.generated.h"

class UItemManagerComponent;
class UUsableItemDataAsset;
class UStaticMeshComponent;

UCLASS()
class ACTIONPRACTICE_API UUseItemAbility : public UActionRecoveryAbility
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UUseItemAbility();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	//GE 적용 타이밍용 커브 이름
	static const FName CurveName_ApplyEffect;

	//캐싱된 아이템 DA (ActivateAbility 시점에 캡처)
	UPROPERTY()
	TObjectPtr<const UUsableItemDataAsset> CachedItemDA = nullptr;

	//GE 적용 태그 (OnGiveAbility에서 캐싱)
	FGameplayTag EffectUsableItemMagnitudeTag;
	FGameplayTag EffectUsableItemDurationTag;

	//GE 적용 여부 (중복 적용 방지)
	bool bEffectApplied = false;

	//사용 중 부착된 소품 메시 컴포넌트
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SpawnedItemMeshComponent = nullptr;

#pragma endregion

#pragma region "Protected Functions"

	virtual void ActivateInitSettings() override;

	//ActionRecoveryAbility 순수 가상 함수 구현
	virtual bool ConsumeStamina() override;
	virtual UAnimMontage* SetMontageToPlayTask() override;
	virtual void SetUpPlayMontageWithEventsTask() override;
	virtual void OnTaskMontageCompleted() override;
	virtual void OnTaskMontageInterrupted() override;

	//커브 에지 핸들러 오버라이드 (ApplyEffect 커브 처리)
	virtual void OnCurveRisingEdgeReceived(FName CurveName) override;

	//GE 적용 로직 (서버 권위)
	void ApplyItemEffect();

	//어빌리티 실행 중 무기 가시성 제어
	void SetWeaponsVisibility(bool bVisible);

	//소품 메시 스폰 및 부착
	void SpawnItemMesh();

	//소품 메시 제거
	void DestroyItemMesh();

#pragma endregion

private:
#pragma region "Private Variables"
#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};
