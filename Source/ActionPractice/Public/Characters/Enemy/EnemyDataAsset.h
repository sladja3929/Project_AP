#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "EnemyDataAsset.generated.h"

class UAnimMontage;

//FName으로 식별되는 공격 데이터
USTRUCT(BlueprintType)
struct FNamedAttackData
{
    GENERATED_BODY()

    //콤보 시퀀스 (1번 공격, 2번 공격, 3번 공격...)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
    TArray<FComboAttackUnit> ComboSequence;
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

            for (const FComboAttackUnit& ComboUnit : AttackData.ComboSequence)
            {
                if (!ComboUnit.AttackMontage.IsNull())
                {
                    AssetsToLoad.Add(ComboUnit.AttackMontage.ToSoftObjectPath());
                }

                if (!ComboUnit.SubAttackMontage.IsNull())
                {
                    AssetsToLoad.Add(ComboUnit.SubAttackMontage.ToSoftObjectPath());
                }
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