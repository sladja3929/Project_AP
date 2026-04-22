#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
    TSoftObjectPtr<UAnimMontage> DeathMontage;

    // ===== HitReaction Montages =====

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReaction")
    TSoftObjectPtr<UAnimMontage> HitReactionLightMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReaction")
    TSoftObjectPtr<UAnimMontage> HitReactionMiddleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReaction")
    TSoftObjectPtr<UAnimMontage> HitReactionHeavyMontage;

    // ===== Groggy Montages =====

    //그로기 시작 (쓰러짐)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy")
    TSoftObjectPtr<UAnimMontage> GroggyStartMontage;

    //그로기 루프 (바닥에서 대기, 루프 몽타주)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy")
    TSoftObjectPtr<UAnimMontage> GroggyLoopMontage;

    //그로기 종료 (일어남)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groggy")
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

    void PreloadAllMontages()
    {
        //이미 프리로드 됐으면 스킵 (LoadedMontageCache가 하드 레퍼런스로 붙잡고 있음)
        if (bMontagesPreloaded) return;

        TArray<FSoftObjectPath> AssetsToLoad;

        for (const FEnemyTaggedAttackData& AttackData : TaggedAttackData)
        {
            for (const FEnemyComboAttackUnit& EnemyCombo : AttackData.ComboSequence)
            {
                if (!EnemyCombo.ComboData.AttackMontage.IsNull())
                {
                    AssetsToLoad.Add(EnemyCombo.ComboData.AttackMontage.ToSoftObjectPath());
                }

                //Charge 유형일 때만 SubAttackMontage 프리로드
                if (AttackData.AttackType == EComboAttackType::Charge && !EnemyCombo.ComboData.SubAttackMontage.IsNull())
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

        //히트리액션 몽타주 프리로드
        if (!HitReactionLightMontage.IsNull())
        {
            AssetsToLoad.Add(HitReactionLightMontage.ToSoftObjectPath());
        }
        if (!HitReactionMiddleMontage.IsNull())
        {
            AssetsToLoad.Add(HitReactionMiddleMontage.ToSoftObjectPath());
        }
        if (!HitReactionHeavyMontage.IsNull())
        {
            AssetsToLoad.Add(HitReactionHeavyMontage.ToSoftObjectPath());
        }

        //그로기 몽타주 프리로드
        if (!GroggyStartMontage.IsNull())
        {
            AssetsToLoad.Add(GroggyStartMontage.ToSoftObjectPath());
        }
        if (!GroggyLoopMontage.IsNull())
        {
            AssetsToLoad.Add(GroggyLoopMontage.ToSoftObjectPath());
        }
        if (!GroggyEndMontage.IsNull())
        {
            AssetsToLoad.Add(GroggyEndMontage.ToSoftObjectPath());
        }

        //Asset Manager를 통한 로딩
        if (AssetsToLoad.Num() > 0 && UAssetManager::IsInitialized())
        {
            UAssetManager& AssetManager = UAssetManager::Get();
            FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();

            //로드된 몽타주를 UPROPERTY 하드 레퍼런스에 보관해야 GC로부터 보호됨
            //StreamableManager.LoadSynchronous는 핸들 없이 호출되면 반환 직후 GC 대상이 되고,
            //TSoftObjectPtr 필드도 WeakObjectPtr 기반이라 하드 레퍼런스를 유지하지 못함
            LoadedMontageCache.Reset(AssetsToLoad.Num());
            for (const FSoftObjectPath& AssetPath : AssetsToLoad)
            {
                if (UObject* Loaded = StreamableManager.LoadSynchronous(AssetPath))
                {
                    if (UAnimMontage* Montage = Cast<UAnimMontage>(Loaded))
                    {
                        LoadedMontageCache.Add(Montage);
                    }
                }
            }
        }

        bMontagesPreloaded = true;
    }

private:
    //PreloadAllMontages로 로드된 몽타주를 GC로부터 보호하기 위한 하드 레퍼런스 캐시
    //쿡 빌드에서 GC가 돌면 프리로드된 몽타주가 수거되어 몽타주 재생이 실패할 수 있기 때문에 DA가 직접 붙잡아 둔다
    UPROPERTY(Transient)
    TArray<TObjectPtr<UAnimMontage>> LoadedMontageCache;

    UPROPERTY(Transient)
    bool bMontagesPreloaded = false;
};