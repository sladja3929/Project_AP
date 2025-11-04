#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "EnemyDataAsset.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FHitSocketInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
    FName HitSocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
    float HitRadius =  10.0f;
};

USTRUCT(BlueprintType)
struct FNamedAttackData
{
    GENERATED_BODY()

    //메인 공격 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> AttackMontage;

    //보조 몽타주 (차지 액션 등, 필요한 경우만 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> SubAttackMontage;
    
    //공격 정보들, 하나의 몽타주에 여러개의 콤보 정보를 넣을 수 있음
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    TArray<FAttackStats> AttackStats;

    //공격 시작점 소켓 정보, 해당 소켓들에서 동시다발적으로 공격 판정 시작
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    TArray<FHitSocketInfo> HitSocketInfo;
};

UCLASS(BlueprintType)
class UEnemyDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Info")
    float BaseDamage = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Definitions")
    TMap<FName, FNamedAttackData> NamedAttackData;
    
    void PreloadAllMontages()
    {
        TArray<FSoftObjectPath> AssetsToLoad;

        for (const TPair<FName, FNamedAttackData>& Pair : NamedAttackData)
        {
            const FNamedAttackData& AttackData = Pair.Value;

            if (!AttackData.AttackMontage.IsNull())
            {
                AssetsToLoad.Add(AttackData.AttackMontage.ToSoftObjectPath());
            }

            if (!AttackData.SubAttackMontage.IsNull())
            {
                AssetsToLoad.Add(AttackData.SubAttackMontage.ToSoftObjectPath());
            }
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