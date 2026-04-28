#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItemDataAsset.h"
#include "GameplayTagContainer.h"
#include "WeaponEnums.h"
#include "AttackData.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "WeaponDataAsset.generated.h"

class UAnimMontage;
class UAnimInstance;

//TMap 대신 사용할 구조체
USTRUCT(BlueprintType)
struct FTaggedAttackData
{
    GENERATED_BODY()

    //이 공격 타입을 식별하는 태그 (예: "공격.일반공격")
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTagContainer AttackTags;

    //공격 유형 (Normal: 기본, Charge: 차지 공격 - SubAttackMontage 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Type")
    EComboAttackType AttackType = EComboAttackType::Normal;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    TSoftObjectPtr<UAnimMontage> GuardBreakMontage;

    //방어 데미지 감소량
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float DamageReduction = 50.0f;

    //가드 강도 (엘든링의 Guard Boost에 해당, 0~100)
    //높을수록 방어 시 스태미나 소모가 줄어든다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float GuardStrength = 50.0f;

    //패리 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry")
    TSoftObjectPtr<UAnimMontage> ParryMontage;

    //패리 판정 각도 (가드 각도와 별도, 정면 기준 편측 각도)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float ParryAngle = 60.0f;
};

UCLASS(BlueprintType)
class UWeaponDataAsset : public UBaseItemDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Info")
    EWeaponEnums WeaponType = EWeaponEnums::None;

    //공격 시작점 소켓 정보
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Info")
    TArray<FHitSocketInfo> HitSocketInfo;

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

    //이 무기의 팔 애니메이션 레이어 ABP (좌/우 팔 인터페이스에 맞는 ABP를 각각 할당)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Layer")
    TSubclassOf<UAnimInstance> LeftArmLayerABP;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Layer")
    TSubclassOf<UAnimInstance> RightArmLayerABP;

    //양손 레이어 ABP (미구현 — 에디터 설정용으로만 노출)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation Layer")
    TSubclassOf<UAnimInstance> TwoHandedLayerABP;

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

        for (FTaggedAttackData& TaggedData : TaggedAttackData)
        {
            for (FComboAttackUnit& ComboUnit : TaggedData.ComboSequence)
            {
                if (!ComboUnit.AttackMontage.IsNull())
                {
                    AssetsToLoad.Add(ComboUnit.AttackMontage.ToSoftObjectPath());
                }

                //Charge 유형일 때만 SubAttackMontage 프리로드
                if (TaggedData.AttackType == EComboAttackType::Charge && !ComboUnit.SubAttackMontage.IsNull())
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

        if (!BlockData.GuardBreakMontage.IsNull())
        {
            AssetsToLoad.Add(BlockData.GuardBreakMontage.ToSoftObjectPath());
        }

        if (!BlockData.ParryMontage.IsNull())
        {
            AssetsToLoad.Add(BlockData.ParryMontage.ToSoftObjectPath());
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
    //WeaponSwitch로 이전 무기 액터가 Destroy되며 GC가 유발되면 프리로드된 몽타주가 수거되어
    //쿡 빌드에서 몽타주 재생이 실패할 수 있기 때문에 DA가 직접 붙잡아 둔다
    UPROPERTY(Transient)
    TArray<TObjectPtr<UAnimMontage>> LoadedMontageCache;

    UPROPERTY(Transient)
    bool bMontagesPreloaded = false;
};