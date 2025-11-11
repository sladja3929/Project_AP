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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Definitions")
    TMap<FName, FNamedAttackData> NamedAttackData;

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