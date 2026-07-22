#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Engine/StreamableManager.h"
#include "EnemyDataAsset.generated.h"

class UAnimMontage;
class UCurveVector;

//적 전용 콤보 단위 (공용 FComboAttackUnit + 적 전용 설정)
USTRUCT(BlueprintType)
struct FEnemyComboAttackUnit
{
    GENERATED_BODY()

    //공용 콤보 데이터 (몽타주, 공격 스탯, 보조 몽타주)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
    FComboAttackUnit ComboData;

    // ===== 적 전용 콤보별 설정 =====

    //타겟 회전 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float RotateTime = 0.1f;

    //콤보 연계 최대 거리
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float MaxTargetDistance = 150.0f;

    //콤보 연계 최대 각도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float MaxTargetAngle = 60.0f;
};

//적 돌진 공격 설정 (타겟 위치 기반 이동)
USTRUCT(BlueprintType)
struct FEnemyLungeConfig
{
    GENERATED_BODY()

    //높이 궤적 커브 (Time 0~1, Z축이 높이 오프셋)
    //Lunging ANS Begin 시점의 적 Z값 기준으로 적용
    //nullptr이면 직선 이동 (지면 대시)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lunge")
    TObjectPtr<UCurveVector> HeightCurve = nullptr;
};

//GameplayTag로 식별되는 적 공격 데이터
USTRUCT(BlueprintType)
struct FEnemyTaggedAttackData
{
    GENERATED_BODY()

    //이 공격을 식별하는 태그 (어빌리티의 Asset Tag와 매칭)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTagContainer AttackTags;

    //공격 유형 (Normal: 기본, Charge: 차지 공격, Lunge: 돌진 공격)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
    EComboAttackType AttackType = EComboAttackType::Normal;

    //콤보 시퀀스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
    TArray<FEnemyComboAttackUnit> ComboSequence;

    //공격별 쿨다운 (0이면 쿨다운 없음)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float CooldownDuration = 0.0f;

    //돌진 설정 (AttackType이 Lunge일 때만 에디터에 표시)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lunge",
        meta = (EditCondition = "AttackType == EComboAttackType::Lunge", EditConditionHides))
    FEnemyLungeConfig LungeConfig;
};

UCLASS(BlueprintType)
class UEnemyDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Info")
    float BaseDamage = 100.0f;

    //공격 시작점 소켓 정보
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Info")
    TArray<FHitSocketInfo> HitSocketInfo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Definitions", meta = (TitleProperty = "AttackTags"))
    TArray<FEnemyTaggedAttackData> TaggedAttackData;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> DeathMontage;

    // ===== HitReaction Montages =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReaction", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> HitReactionLightMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReaction", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> HitReactionMiddleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReaction", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> HitReactionHeavyMontage;

    // ===== Groggy Montages =====

    //그로기 시작 (쓰러짐)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> GroggyStartMontage;

    //그로기 루프 (바닥에서 대기, 루프 몽타주)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> GroggyLoopMontage;

    //그로기 종료 (일어남)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> GroggyEndMontage;

    //그로기 루프 지속 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy")
    float GroggyLoopDuration = 3.0f;

    //AttackTags와 일치하는 공격 데이터를 반환
    const FEnemyTaggedAttackData* GetAttackDataByTags(const FGameplayTagContainer& AttackTags) const
    {
        for (const FEnemyTaggedAttackData& Data : TaggedAttackData)
        {
            if (Data.AttackTags == AttackTags)
            {
                return &Data;
            }
        }
        return nullptr;
    }

    //GetOptions용 함수 - HitSocketInfo에서 소켓 그룹 이름들을 반환
    UFUNCTION()
    TArray<FString> GetSocketGroupNames() const
    {
        TArray<FString> Names;
        for (const FHitSocketInfo& Info : HitSocketInfo)
        {
            if (Info.HitSocketName != NAME_None)
            {
                Names.Add(Info.HitSocketName.ToString());
            }
        }
        return Names;
    }

    //AssetManager PrimaryAssetTypesToScan의 "EnemyData" 타입과 매핑 (Combat 번들 ChangeBundleState용)
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("EnemyData"), GetFName());
    }

    //모든 몽타주를 Combat 번들로 비동기 배치 프리로드 (공격 콤보 + 사망 + 히트리액션 + 그로기 전부)
    //AEnemyCharacter::BeginPlay에서 fire-and-forget으로 호출된다 (호출 시그니처 파라미터 없음 유지)
    void PreloadAllMontages();

    virtual void BeginDestroy() override;

private:
    //Combat 번들 로드 핸들 — UPROPERTY 아닌 순수 C++ 멤버
    //ChangeBundleStateForPrimaryAssets가 반환하는 핸들을 보관해 AssetManager 참조 유지 시맨틱에
    //의존하지 않고 로드된 몽타주 참조를 확정적으로 붙잡는다 (에셋 N개 배열 캐시 → 번들 핸들 1개로 축소)
    TSharedPtr<FStreamableHandle> BundleHandle;

    //중복 프리로드 요청 방지 플래그
    bool bPreloadRequested = false;
};