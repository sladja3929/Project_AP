#pragma once

#include "CoreMinimal.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "ActionPracticeAbilitySystemComponent.generated.h"

class AActionPracticeCharacter;
class UActionPracticeAttributeSet;

//APASC 내부 방어 판정 상태
enum class EPlayerDefenseState : uint8
{
	None,                    //방어 없음 (일반 피격)
	Blocked,                 //가드 성공
	GuardBroken,             //가드 브레이크
	Parried,                 //패리 성공
	ParryFallbackBlocked,    //패리 실패 → 가드 폴백 성공
	ParryFallbackGuardBroken,//패리 실패 → 가드 폴백 → 가드 브레이크
};

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

	//===== Death =====
	virtual void HandleDeath() override;

	//===== Combat State =====
	//교전 중인 적 등록·해제 (서버, EnemyAIController 전투 진입·해제 시 호출). 하나라도 남아 있으면 State.InCombat 부여
	void SetEngagedByEnemy(AActor* Enemy, bool bEngaged);

	//전투 상태 여부 — State.InCombat은 소유 클라이언트까지 복제되어 예측 경로에서도 서버와 같은 판정
	bool IsInCombat() const;

	//===== Defense Policy Override =====
	virtual void CalculateAndSetAttributes(AActor* SourceActor, const FFinalAttackData& FinalAttackData) override;
	virtual void PrepareHitReactionEventData(FGameplayEventData& OutEventData, const FFinalAttackData& FinalAttackData) override;

#pragma endregion

protected:
#pragma region "Protected Variables"

	TObjectPtr<AActionPracticeCharacter> OwnerCharacter;
	FGameplayTag StateAbilityBlockingTag;
	FGameplayTag StateGuardBrokenTag;
	FGameplayTag StateParryingTag;

	//가드로 스태미나를 소모했을 때 적용할 리젠 차단 GE (State.StaminaRegenBlocked 부여, HasDuration)
	UPROPERTY(EditDefaultsOnly, Category = "Defense|Stamina")
	TSubclassOf<UGameplayEffect> GuardStaminaRegenBlockEffect;

	//가드 스태미나 소모 후 리젠 차단 시간(초). 0 이하면 GE에 설정된 지속시간 사용
	UPROPERTY(EditDefaultsOnly, Category = "Defense|Stamina", meta = (ClampMin = "0.0", Units = "s"))
	float GuardStaminaRegenBlockDuration = 0.5f;

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//포이즈 브레이크 + 가드 브레이크 시 HitReaction 활성화
	virtual bool ShouldActivateHitReaction() const override;

	virtual EDefenseResult GetDefenseResult() const override;

#pragma endregion

private:
#pragma region "Private Variables"

	//내부 방어 판정 상태
	EPlayerDefenseState LastDefenseState = EPlayerDefenseState::None;

	//이 플레이어와 교전 중인 적 (서버 전용, 키는 적의 AIController)
	TSet<TWeakObjectPtr<AActor>> EngagedEnemies;

#pragma endregion

#pragma region "Private Functions"

	//HandleGameplayEvent 계열 태스크용 RPC
	UFUNCTION(Server, Reliable)
	void Server_HandleGameplayEvent(const FGameplayEventData_NetPredicted& Payload);

	void CheckBlockSuccess(AActor* SourceActor);
	void CheckParrySuccess(AActor* SourceActor, const FFinalAttackData& FinalAttackData);

	//패리 성공 시 적 강제 그로기 발동
	void ForceEnemyGroggy(AActor* EnemyActor);

	//가드 스태미나 소모 후 일정 시간 리젠 차단
	void ApplyGuardStaminaRegenBlock();

	//EngagedEnemies 정리 후 State.InCombat 태그 동기화
	void RefreshCombatStateTag();

#pragma endregion
};
