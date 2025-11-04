#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeaponEnums.h"
#include "AttackData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "WeaponDataAsset.generated.h"

class UAnimMontage;

//하나의 콤보 단위 (몽타주 - 데이터 - 보조몽타주를 하나로 묶음)
USTRUCT(BlueprintType)
struct FComboAttackUnit
{
    GENERATED_BODY()

    //메인 공격 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> AttackMontage;

    //이 콤보의 공격 데이터
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    FAttackStats AttackData;

    //보조 몽타주 (차지 액션 등, 필요한 경우만 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> SubAttackMontage;
};

//TMap 대신 사용할 구조체
USTRUCT(BlueprintType)
struct FTaggedAttackData
{
    GENERATED_BODY()

    //이 공격 타입을 식별하는 태그 (예: "공격.일반공격")
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTagContainer AttackTags;

    //콤보 시퀀스 (1번 공격, 2번 공격, 3번 공격...)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
    TArray<FComboAttackUnit> ComboSequence;
};

//방어 정보
USTRUCT(BlueprintType)
struct FBlockActionData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> BlockIdleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> BlockReactionLightMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> BlockReactionMiddleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> BlockReactionHeavyMontage;

    //방어 데미지 감소량
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float DamageReduction = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block")
    float StaminaCost = 10.0f;
};

UCLASS(BlueprintType)
class UWeaponDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Info")
    EWeaponEnums WeaponType = EWeaponEnums::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Info")
    int32 HitSocketCount = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Info")
    float HitRadius = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stats")
    float BaseDamage = 100.0f;

    //근력 보정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stats", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float StrengthScaling = 60.0f;
    
    //기량 보정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stats", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float DexterityScaling = 60.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Definitions")
    TArray<FTaggedAttackData> TaggedAttackData;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Definitions")
    FBlockActionData BlockData;
    
    void PreloadAllMontages()
    {
        TArray<FSoftObjectPath> AssetsToLoad;

        for (FTaggedAttackData& TaggedData : TaggedAttackData)
        {
            for (FComboAttackUnit& ComboUnit : TaggedData.ComboSequence)
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

        if (!BlockData.BlockIdleMontage.IsNull())
        {
            AssetsToLoad.Add(BlockData.BlockIdleMontage.ToSoftObjectPath());
        }

        if (!BlockData.BlockReactionLightMontage.IsNull())
        {
            AssetsToLoad.Add(BlockData.BlockReactionLightMontage.ToSoftObjectPath());
        }

        if (!BlockData.BlockReactionMiddleMontage.IsNull())
        {
            AssetsToLoad.Add(BlockData.BlockReactionMiddleMontage.ToSoftObjectPath());
        }

        if (!BlockData.BlockReactionHeavyMontage.IsNull())
        {
            AssetsToLoad.Add(BlockData.BlockReactionHeavyMontage.ToSoftObjectPath());
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