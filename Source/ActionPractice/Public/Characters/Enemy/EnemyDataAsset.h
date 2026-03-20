#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "EnemyDataAsset.generated.h"

class UAnimMontage;

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

//GameplayTag로 식별되는 적 공격 데이터
USTRUCT(BlueprintType)
struct FEnemyTaggedAttackData
{
    GENERATED_BODY()

    //이 공격을 식별하는 태그 (어빌리티의 Asset Tag와 매칭)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTagContainer AttackTags;

    //콤보 시퀀스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
    TArray<FEnemyComboAttackUnit> ComboSequence;

    //공격별 쿨다운 (0이면 쿨다운 없음)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float CooldownDuration = 0.0f;
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
    TSoftObjectPtr<UAnimMontage> DeathMontage;

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

    void PreloadAllMontages()
    {
        TArray<FSoftObjectPath> AssetsToLoad;

        for (const FEnemyTaggedAttackData& AttackData : TaggedAttackData)
        {
            for (const FEnemyComboAttackUnit& EnemyCombo : AttackData.ComboSequence)
            {
                if (!EnemyCombo.ComboData.AttackMontage.IsNull())
                {
                    AssetsToLoad.Add(EnemyCombo.ComboData.AttackMontage.ToSoftObjectPath());
                }

                if (!EnemyCombo.ComboData.SubAttackMontage.IsNull())
                {
                    AssetsToLoad.Add(EnemyCombo.ComboData.SubAttackMontage.ToSoftObjectPath());
                }
            }
        }

        //사망 몽타주 프리로드
        if (!DeathMontage.IsNull())
        {
            AssetsToLoad.Add(DeathMontage.ToSoftObjectPath());
        }

        //Asset Manager를 통한 로딩
        if (AssetsToLoad.Num() > 0 && UAssetManager::IsInitialized())
        {
            UAssetManager& AssetManager = UAssetManager::Get();
            FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();

            //동기 로딩
            for (const FSoftObjectPath& AssetPath : AssetsToLoad)
            {
                StreamableManager.LoadSynchronous(AssetPath);
            }
        }
    }
};