#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "EnemyPack.generated.h"

class AActor;
class AEnemyAIController;
class AEnemyWanderArea;

/**
 * 같은 타깃과 싸우는 적 무리 (서버 전용, UEnemyPackSubsystem이 생성·소유)
 * - 구성원은 전투 중인 EnemyAIController. 전투 해제·사망·리셋 시 컨트롤러가 스스로 탈퇴
 * - 공격권: 타깃에 가까운 순으로 Settings.MaxAttackers명. 기존 공격자는 비공격자가 AttackerSwapMargin 이상 더 가까울 때만 교체
 * - 전투 해제 판단: 살아 있는 구성원 전원이 해제 거리 밖일 때만 해제 (한 마리만 멀어져도 무리 타깃 유지)
 * - 공격권은 행동 선택용 플래그일 뿐 진행 중인 공격을 끊지 않음 (StateTree가 공격 노드 종료 후 다시 고름)
 */
UCLASS()
class ACTIONPRACTICE_API UEnemyPack : public UObject
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

#pragma endregion

#pragma region "Public Functions"

	void Initialize(AActor* InTarget, AEnemyWanderArea* InArea, FName InEnemyName, const FEnemyPackSettings& InSettings);

	void AddMember(AEnemyAIController* Member);
	void RemoveMember(const AEnemyAIController* Member);

	//유효하지 않은 구성원(파괴·빙의 해제) 정리
	void PruneInvalidMembers();

	//공격자 재지정 (빈 자리는 가까운 순으로 채우고, 여유 거리 이상 가까운 비공격자와 가장 먼 공격자 교체)
	void UpdateAttackers();

	bool IsAttacker(const AEnemyAIController* Member) const;

	//살아 있는 구성원 전원이 타깃과 Distance보다 먼지 (수평 거리)
	bool AreAllMembersBeyond(float Distance) const;

	//Location에서 Radius 안에 구성원이 있는지 (수평 거리)
	bool HasMemberWithin(const FVector& Location, float Radius) const;

	FORCEINLINE AActor* GetTarget() const { return Target.Get(); }
	FORCEINLINE AEnemyWanderArea* GetArea() const { return Area.Get(); }
	FORCEINLINE FName GetEnemyName() const { return EnemyName; }
	FORCEINLINE const FEnemyPackSettings& GetSettings() const { return Settings; }
	FORCEINLINE const TArray<TWeakObjectPtr<AEnemyAIController>>& GetMembers() const { return Members; }
	FORCEINLINE int32 GetMemberCount() const { return Members.Num(); }
	FORCEINLINE int32 GetAttackerCount() const { return Attackers.Num(); }
	FORCEINLINE bool IsEmpty() const { return Members.IsEmpty(); }

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

#pragma endregion

private:
#pragma region "Private Variables"

	TWeakObjectPtr<AActor> Target;

	//무리 기준 배회 구역 (구역 없이 반경으로 모인 무리는 nullptr)
	TWeakObjectPtr<AEnemyWanderArea> Area;

	//구역 없는 무리의 합류 판정용 적 종류 (AEnemyCharacter::EnemyName)
	FName EnemyName = NAME_None;

	FEnemyPackSettings Settings;

	TArray<TWeakObjectPtr<AEnemyAIController>> Members;

	//공격권 보유 구성원 (Members의 부분 집합)
	TArray<TWeakObjectPtr<AEnemyAIController>> Attackers;

#pragma endregion

#pragma region "Private Functions"

	//구성원 폰과 타깃의 수평 거리 (폰이 없으면 음수)
	float GetMemberDistance(const AEnemyAIController* Member) const;

#pragma endregion
};
