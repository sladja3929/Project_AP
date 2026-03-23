#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DefensePolicy.h"
#include "BaseAbilitySystemComponent.generated.h"

class ABaseCharacter;
class UAttributeSet;
struct FFinalAttackData;
struct FActionPracticeGameplayEffectContext;

/**
 * Base AbilitySystemComponent
 * ActionPracticeAbilitySystemComponent와 BossAbilitySystemComponent의 공통 기능
 */
UCLASS()
class ACTIONPRACTICE_API UBaseAbilitySystemComponent : public UAbilitySystemComponent, public IDefensePolicy
{
	GENERATED_BODY()

public:
#pragma region "Public Functions"

	UBaseAbilitySystemComponent();

	//ASC가 Owner/Avatar 정보를 수집하는 지점
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	//초기화 완료 델리게이트(필요 시 외부에서 바인딩)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnASCInitialized, UBaseAbilitySystemComponent*);
	FOnASCInitialized OnASCInitialized;

	//EventData를 포함하여 어빌리티 활성화 시도
	//TryActivateAbility와 동일하되 TriggerEventData를 전달
	bool TryActivateAbilityWithEventData(FGameplayAbilitySpecHandle AbilityToActivate, const FGameplayEventData* TriggerEventData);

	//기본 GE 생성 헬퍼
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	FGameplayEffectSpecHandle CreateGameplayEffectSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, UObject* SourceObject = nullptr);

	//공격 GE 생성 (C++ 전용 — FHitResult 포인터 파라미터로 UFUNCTION 불가)
	FGameplayEffectSpecHandle CreateAttackGameplayEffectSpec(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		float Level,
		UObject* SourceObject,
		const FFinalAttackData& FinalAttackData,
		const FHitResult* HitResult = nullptr
	);

	//SetByCaller 설정
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	void SetSpecSetByCallerMagnitude(FGameplayEffectSpecHandle& SpecHandle, const FGameplayTag& Tag, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	void SetSpecSetByCallerMagnitudes(FGameplayEffectSpecHandle& SpecHandle, const TMap<FGameplayTag, float>& Magnitudes);

	//===== Death =====
	//Health <= 0 시 HandleOnDamagedResolved에서 호출되는 공통 진입점
	//파생 ASC에서 오버라이드하여 플레이어/보스 각자의 죽음 처리를 분기
	virtual void HandleDeath();

	//리스폰 후 다시 사망 가능 상태로 복구
	void ResetDeathHandled();

	//사망 처리 완료 여부 조회
	bool IsDeathHandled() const { return bDeathHandled; }

	//===== Defense Policy Interface =====
	UFUNCTION()
	virtual void OnDamaged(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

	UFUNCTION()
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

	UFUNCTION()
	virtual void HandleOnDamagedResolved(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;

	virtual void PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TWeakObjectPtr<ABaseCharacter> CachedCharacter;

	FGameplayTag AbilityHitReactionTag;

	//죽음 중복 진입 방지 가드
	bool bDeathHandled = false;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//HitReaction을 활성화할지 여부 (포이즈 브레이크 체크)
	//파생 클래스에서 오버라이드하여 추가 조건을 포함할 수 있음
	virtual bool ShouldActivateHitReaction() const;

	//브레이크 게이지 리셋 (Poise 등)
	//HandleOnDamagedResolved에서 음수값이 사용된 후 호출
	//파생 클래스에서 오버라이드하여 추가 게이지(Stance 등) 리셋 가능
	virtual void ResetBreakGauges();

#pragma endregion

private:
#pragma region "Private Variables"

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};