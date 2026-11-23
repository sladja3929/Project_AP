#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Engine/StreamableManager.h"
#include "EnemyDataAsset.generated.h"

class UAnimMontage;
class UCurveVector;
class AEnemyProjectile;

//적 전용 콤보 단위 (공용 FComboAttackUnit + 적 전용 설정)
USTRUCT(BlueprintType)
struct FEnemyComboAttackUnit
{
    GENERATED_BODY()

    //공용 콤보 데이터 (몽타주, 공격 스탯, 보조 몽타주)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
    FComboAttackUnit ComboData;

    // ===== 적 전용 콤보별 설정 =====
    //타겟 회전은 몽타주의 AttackTarget 워프 윈도우(MotionWarping ANS)에서 정의

    //콤보 연계 전용: 이 유닛에서 다음 유닛으로 이어갈 수 있는 최대 거리 (마지막 유닛·1단 콤보에서는 미사용)
    //공격 시작 거리·각도는 StateTree(행동 그룹 진입 조건)가 판정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float MaxTargetDistance = 150.0f;

    //콤보 연계 전용: 이 유닛에서 다음 유닛으로 이어갈 수 있는 최대 각도 (마지막 유닛·1단 콤보에서는 미사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    float MaxTargetAngle = 60.0f;

    //AttackApproach 워프(이동) 정지 거리. 타깃과 오너의 액터 중심 간 수평 거리 (캡슐 반경 미포함)
    //몽타주에 AttackApproach MotionWarping ANS가 있을 때만 사용. 이보다 가까우면(시작 시 또는 워프 도중) 이동 워프 비활성
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0.0"))
    float WarpStopDistance = 120.0f;
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

//적 투사체 공격 설정 (Projectile Spawn → (Grab) → Launch 노티파이 순서로 생성·부착·타깃 방향 포물선 발사)
//투사체 고유 속성(메시·판정 반경·착탄 반경·착탄 큐)은 투사체 BP 기본값, 데미지 수치는 ComboSequence의 AttackData가 담당
USTRUCT(BlueprintType)
struct FEnemyProjectileConfig
{
    GENERATED_BODY()

    //생성할 투사체 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    TSubclassOf<AEnemyProjectile> ProjectileClass;

    //발사 전까지 투사체를 붙여 둘 메시 소켓(본) 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    FName AttachSocketName = NAME_None;

    //(선택) 양손으로 드는 투사체의 두 번째 소켓. 지정하면 AttachSocketName과의 중간 위치를 따라감
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    FName SecondaryAttachSocketName = NAME_None;

    //true면 Spawn 노티파이에서 오너 앞 등장 위치에 놓았다가 Grab 노티파이에서 손에 부착 (등장 연출은 투사체 BP의 단계 이벤트)
    //false면 Spawn 노티파이에서 바로 손에 부착
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    bool bPresentBeforeGrab = false;

    //등장 위치 (오너 액터 로컬 공간, 액터 위치 기준). Grab 프레임의 손 소켓 위치에 맞춰야 부착 순간 튀지 않음
    //등장 ~ Grab 사이에는 회전 워프를 걸지 않을 것 (오너가 돌면 손과 등장 위치가 어긋남)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bPresentBeforeGrab"))
    FVector PresentOffset = FVector(150.0f, 0.0f, 0.0f);

    //수평 비행 속도 (cm/s). 비행 시간 = 수평 거리 / 이 값을 Min~Max FlightTime으로 클램프
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1.0"))
    float HorizontalSpeed = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.05"))
    float MinFlightTime = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.05"))
    float MaxFlightTime = 1.5f;

    //타깃 이동 예측 비율 (0: 발사 시점 위치, 1: 비행 시간 동안 현재 속도로 이동할 위치). 1 미만이어야 옆으로 움직여 피할 수 있음
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LeadFactor = 0.5f;

    //조준 지점 높이 오프셋 (타깃 액터 위치 = 캡슐 중심 기준)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
    float TargetHeightOffset = 0.0f;
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

    //투사체 설정 (AttackType이 Projectile일 때만 에디터에 표시)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile",
        meta = (EditCondition = "AttackType == EComboAttackType::Projectile", EditConditionHides))
    FEnemyProjectileConfig ProjectileConfig;
};

//무리 전투 설정 (UEnemyPack이 소집·공격권 분배에 사용)
USTRUCT(BlueprintType)
struct FEnemyPackSettings
{
    GENERATED_BODY()

    //무리 전투 사용 여부. 켜면 전투 진입 시 같은 배회 구역(구역이 없으면 RallyRadius 안 같은 종류) 적을 함께 전투로 끌어들임
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pack")
    bool bEnablePack = false;

    //동시에 공격권을 가질 수 있는 최대 구성원 수 (타깃에 가까운 순)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pack", meta = (EditCondition = "bEnablePack", ClampMin = "1"))
    int32 MaxAttackers = 2;

    //공격자 교체 여유 거리. 비공격자가 가장 먼 공격자보다 이만큼 이상 가까울 때만 교체 (거리가 비슷할 때 공격권이 오가는 떨림 방지)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pack", meta = (EditCondition = "bEnablePack", ClampMin = "0.0"))
    float AttackerSwapMargin = 100.0f;

    //배회 구역이 없는 적의 소집 반경 (전투 진입한 적 기준 수평 거리)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pack", meta = (EditCondition = "bEnablePack", ClampMin = "0.0"))
    float RallyRadius = 2000.0f;
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

    // ===== Pack =====

    //무리 전투 설정 (늑대 등 무리 단위로 싸우는 적만 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pack")
    FEnemyPackSettings PackSettings;

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

    //AttackTags·콤보 인덱스의 공격 스탯으로 히트 판정에 넘길 최종 공격 데이터 조립 (인덱스는 범위로 클램프)
    //본체 판정(EnemyAttackComponent)과 투사체 판정(EnemyProjectile)이 같은 계산을 공유
    bool BuildFinalAttackData(const FGameplayTagContainer& AttackTags, int32 ComboIndex, FFinalAttackData& OutAttackData) const;

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