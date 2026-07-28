#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItemDataAsset.h"
#include "GameplayTagContainer.h"
#include "WeaponEnums.h"
#include "AttackData.h"
#include "Engine/StreamableManager.h"
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
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> BlockIdleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> BlockReactionLightMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> BlockReactionMiddleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> BlockReactionHeavyMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AssetBundles = "Combat"))
    TSoftObjectPtr<UAnimMontage> GuardBreakMontage;

    //방어 데미지 감소량
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float DamageReduction = 50.0f;

    //가드 강도 (엘든링의 Guard Boost에 해당, 0~100)
    //높을수록 방어 시 스태미나 소모가 줄어든다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float GuardStrength = 50.0f;

    //패리 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry", meta = (AssetBundles = "Combat"))
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

    //AssetManager PrimaryAssetTypesToScan의 "WeaponData" 타입과 매핑 (Combat 번들 ChangeBundleState용)
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("WeaponData"), GetFName());
    }

    //모든 무기 몽타주를 Combat 번들로 비동기 배치 프리로드 (콤보 AttackMontage + Charge SubAttackMontage + 블록/패리류 전부)
    //AWeapon::BeginPlay / UAttackSequenceAbility::CacheWeaponData에서 fire-and-forget으로 호출된다
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