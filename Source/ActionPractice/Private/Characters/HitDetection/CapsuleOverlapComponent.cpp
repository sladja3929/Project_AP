#include "Characters/HitDetection/CapsuleOverlapComponent.h"
#include "Items/Weapon.h"
#include "Items/WeaponDataAsset.h"
#include "Characters/ActionPracticeCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogCapsuleOverlapComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogCapsuleOverlapComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UCapsuleOverlapComponent::UCapsuleOverlapComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    //오버랩 이벤트
    OnComponentBeginOverlap.AddDynamic(this, &UCapsuleOverlapComponent::OnCapsuleBeginOverlap);
}

void UCapsuleOverlapComponent::BeginPlay()
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCollisionObjectType(ECC_GameTraceChannel1); //WeaponTrace(HitDetection)
    SetCollisionResponseToAllChannels(ECR_Ignore);
    //AttackTrace와 동일하게 PhysicsBody 메시 바디만 판정
    //적 히트 바디(HItDetectionPhysics 프로파일)는 ObjectType=PhysicsBody이며 HitDetection 채널을 Block함
    //적 루트 캡슐(Pawn 프로파일)은 HitDetection 채널을 Ignore하므로 오버랩 대상이 아님 → 바디 단위 정합
    SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    //오버랩 이벤트는 양쪽 컴포넌트 모두 활성화되어야 발생 (캡슐은 기본 true이나 명시)
    SetGenerateOverlapEvents(true);
    
    
    OwnerWeapon = Cast<AWeapon>(GetOwner());
    if (!OwnerWeapon)
    {
        DEBUG_LOG(TEXT("CapsuleOverlapComponent: Owner is not a weapon!"));
        return;
    }

    const UWeaponDataAsset* WeaponData = OwnerWeapon->GetWeaponData();
    UStaticMeshComponent* WeaponMesh = OwnerWeapon->FindComponentByClass<UStaticMeshComponent>();
    if (WeaponMesh)
    {
        AttachToComponent(WeaponMesh, FAttachmentTransformRules::KeepRelativeTransform);
        
        // 소켓 기반으로 캡슐 높이만 계산 (첫 번째 소켓 정보 사용)
        if (WeaponData && WeaponData->HitSocketInfo.Num() > 0)
        {
            const FHitSocketInfo& FirstSocketInfo = WeaponData->HitSocketInfo[0];
            if (FirstSocketInfo.HitSocketCount >= 2)
            {
                FName FirstSocket = FName(*FString::Printf(TEXT("%s_0"),
                                                           *FirstSocketInfo.HitSocketName.ToString()));
                FName LastSocket = FName(*FString::Printf(TEXT("%s_%d"),
                                                          *FirstSocketInfo.HitSocketName.ToString(),
                                                          FirstSocketInfo.HitSocketCount - 1));

                if (WeaponMesh->DoesSocketExist(FirstSocket) &&
                    WeaponMesh->DoesSocketExist(LastSocket))
                {
                    FVector FirstPos = WeaponMesh->GetSocketLocation(FirstSocket);
                    FVector LastPos = WeaponMesh->GetSocketLocation(LastSocket);

                    // 무기 길이를 캡슐 높이로 설정
                    float WeaponLength = (FirstPos - LastPos).Size();
                    DefaultCapsuleHalfHeight = WeaponLength * 0.5f;

                    // 캡슐을 무기 중심에 배치
                    FVector CenterPos = (FirstPos + LastPos) * 0.5f;
                    FVector LocalCenter = WeaponMesh->GetComponentTransform().InverseTransformPosition(CenterPos);
                    SetRelativeLocation(LocalCenter);

                    // 무기 방향에 맞춰 회전
                    FVector WeaponDirection = (FirstPos - LastPos).GetSafeNormal();
                    FRotator CapsuleRotation = FRotationMatrix::MakeFromZ(WeaponDirection).Rotator();
                    SetRelativeRotation(CapsuleRotation);
                }
            }
        }
        else
        {
            // 소켓이 없으면 기본 위치
            SetRelativeLocation(FVector(0, 0, DefaultCapsuleHalfHeight));
            SetRelativeRotation(FRotator(90.0f, 0, 0));
        }
    }
    
    // 고정 크기로 캡슐 설정
    SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
    
    DEBUG_LOG(TEXT("CapsuleOverlap initialized - Radius: %.2f (fixed), HalfHeight: %.2f"),
              DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
    
    Super::BeginPlay();
}

void UCapsuleOverlapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsDetecting)
    {
        //윈도우 동안 캡슐이 이동한 프레임 수 집계 (드로잉 on/off와 무관하게 항상 카운트)
        ++DebugOverlapUpdateCounter;

        //디버그 드로잉 전용 (판정은 OnComponentBeginOverlap에서 수행)
        if (bDrawDebugCapsule)
        {
            DrawDebugCapsuleShape();
        }
    }
}

#pragma region "HitDetectionInterface Implementation"
void UCapsuleOverlapComponent::PrepareHitDetection(const FGameplayTagContainer& AttackTags, const int32 ComboIndex)
{
    CurrentComboIndex = ComboIndex;
    
    if (!LoadAttackConfig(AttackTags, ComboIndex))
    {
        DEBUG_LOG(TEXT("Failed to load attack config for tag container"));
        return;
    }
    
    ResetHitActors();
    BindEventCallbacks();
    
    bIsPrepared = true;
    
    DEBUG_LOG(TEXT("PrepareHitDetection - Attack Tags Count: %d, Combo: %d"), 
              AttackTags.Num(), ComboIndex);
}

void UCapsuleOverlapComponent::HandleHitDetectionStart(const FGameplayEventData& Payload)
{
    if (!bIsPrepared)
    {
        DEBUG_LOG(TEXT("HitDetectionStart - Not Prepared"));
        return;
    }
    
    //충돌 활성화
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    bIsDetecting = true;
    SetComponentTickEnabled(true);

    DebugOverlapUpdateCounter = 0;

    DEBUG_LOG(TEXT("HitDetection Started - Overlap Active"));
}

void UCapsuleOverlapComponent::HandleHitDetectionEnd(const FGameplayEventData& Payload)
{
    //충돌 비활성화 (Tick off)
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    bIsDetecting = false;
    SetComponentTickEnabled(false);
    //프레임 드랍으로 HitDetectionEnd가 HitDetectionStart보다 먼저 도착할 수 있기 때문에
    //bIsPrepared를 여기서 false로 설정하지 않고 다음 PrepareHitDetection에서 관리
    //언바인딩도 여기서 하지 않고 EndPlay에서만 수행 (AttackTraceComponent와 동일 정책)

    //AttackTrace의 StopTrace 카운터 로그와 대칭
    DEBUG_LOG(TEXT("HitDetection Ended - Overlap update frames: %d"), DebugOverlapUpdateCounter);
}
#pragma endregion

#pragma region "Event Binding"
void UCapsuleOverlapComponent::BindEventCallbacks()
{
    if (!OwnerWeapon) return;
    
    AActionPracticeCharacter* Character = OwnerWeapon->GetOwnerCharacter();
    if (!Character) return;
    
    CachedASC = Character->GetAbilitySystemComponent();
    if (!CachedASC) return;
    
    //기존 핸들 정리
    UnbindEventCallbacks();
    
    //이벤트 구독
    HitDetectionStartHandle = CachedASC->GenericGameplayEventCallbacks
        .FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag())
        .AddLambda([this](const FGameplayEventData* EventData)
        {
            if (IsValid(this) && EventData)
            {
                HandleHitDetectionStart(*EventData);
            }
        });
    
    HitDetectionEndHandle = CachedASC->GenericGameplayEventCallbacks
        .FindOrAdd(UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag())
        .AddLambda([this](const FGameplayEventData* EventData)
        {
            if (IsValid(this) && EventData)
            {
                HandleHitDetectionEnd(*EventData);
            }
        });
}

void UCapsuleOverlapComponent::UnbindEventCallbacks()
{
    if (CachedASC)
    {
        if (HitDetectionStartHandle.IsValid())
        {
            if (auto* Delegate = CachedASC->GenericGameplayEventCallbacks.Find(
                UGameplayTagsSubsystem::GetEventNotifyHitDetectionStartTag()))
            {
                Delegate->Remove(HitDetectionStartHandle);
            }
        }
        
        if (HitDetectionEndHandle.IsValid())
        {
            if (auto* Delegate = CachedASC->GenericGameplayEventCallbacks.Find(
                UGameplayTagsSubsystem::GetEventNotifyHitDetectionEndTag()))
            {
                Delegate->Remove(HitDetectionEndHandle);
            }
        }
    }
    
    HitDetectionStartHandle.Reset();
    HitDetectionEndHandle.Reset();
}
#pragma endregion

#pragma region "Collision Handling"
void UCapsuleOverlapComponent::OnCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                AActor* OtherActor,
                                                UPrimitiveComponent* OtherComp,
                                                int32 OtherBodyIndex,
                                                bool bFromSweep,
                                                const FHitResult& SweepResult)
{
	//히트 판정은 서버에서만 (싱글플레이어에서는 항상 true)
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

    if (!bIsDetecting || !OtherActor) return;

    //HitResult 구성 (Sweep이면 그대로, 아니면 수동 구성)
    FHitResult HitResult;

    if (bFromSweep)
    {
        //Sweep 결과 그대로 사용
        HitResult = SweepResult;
    }
    else
    {
        //Overlap의 경우 수동으로 HitResult 구성
        HitResult.HitObjectHandle = FActorInstanceHandle(OtherActor);
        HitResult.Component = OtherComp;
        HitResult.Location = GetComponentLocation();
        HitResult.ImpactPoint = GetComponentLocation();
        HitResult.Normal = (OtherActor->GetActorLocation() - GetComponentLocation()).GetSafeNormal();
        HitResult.ImpactNormal = HitResult.Normal;
        HitResult.Distance = 0.0f;
        HitResult.bBlockingHit = false;
        HitResult.bStartPenetrating = true;

        //본 정보가 필요한 경우
        if (OtherComp)
        {
            HitResult.BoneName = NAME_None;
            HitResult.FaceIndex = INDEX_NONE;
        }
    }

    if (ValidateHit(OtherActor, HitResult, bCurrentIsMultiHit))
    {
        ProcessHit(OtherActor, HitResult);
    }
}

bool UCapsuleOverlapComponent::ValidateHit(AActor* HitActor, const FHitResult& HitResult, bool bIsMultiHit)
{
    if (!HitActor || !OwnerWeapon) return false;

    //자기 자신과 소유자 제외
    AActionPracticeCharacter* WeaponOwner = OwnerWeapon->GetOwnerCharacter();
    if (HitActor == OwnerWeapon || HitActor == WeaponOwner) return false;

    //중복 체크 (AttackTraceComponent::ValidateHit과 동일 계약)
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (FHitValidationData* ValidationData = HitValidationMap.Find(HitActor))
    {
        //다단히트가 아닐 경우, 이미 있으면 리턴
        if (!bIsMultiHit) return false;

        if (CurrentTime - ValidationData->LastHitTime < HitCooldownTime)
        {
            return false;
        }

        ValidationData->LastHitTime = CurrentTime;
        ValidationData->HitCount++;
    }
    else
    {
        FHitValidationData NewData;
        NewData.HitActor = HitActor;
        NewData.LastHitTime = CurrentTime;
        NewData.HitCount = 1;
        HitValidationMap.Add(HitActor, NewData);
    }

    return true;
}

void UCapsuleOverlapComponent::ProcessHit(AActor* HitActor, const FHitResult& HitResult)
{
    DEBUG_LOG(TEXT("Overlap Hit: %s at %s"),
              *HitActor->GetName(),
              *HitResult.Location.ToString());

    //이벤트 브로드캐스트
    OnWeaponHit.Broadcast(HitActor, HitResult, CurrentAttackData);
}

void UCapsuleOverlapComponent::ResetHitActors()
{
    HitValidationMap.Empty();
}
#pragma endregion

#pragma region "Configuration"
bool UCapsuleOverlapComponent::LoadAttackConfig(const FGameplayTagContainer& AttackTags, int32 ComboIndex)
{
    if (!OwnerWeapon) return false;

    const UWeaponDataAsset* WeaponData = OwnerWeapon->GetWeaponData();
    if (!WeaponData) return false;

    const FTaggedAttackData* AttackData = OwnerWeapon->GetWeaponAttackDataByTag(AttackTags);
    if (!AttackData || AttackData->ComboSequence.Num() == 0) return false;

    ComboIndex = FMath::Clamp(ComboIndex, 0, AttackData->ComboSequence.Num() - 1);
    const FAttackStats& AttackInfo = AttackData->ComboSequence[ComboIndex].AttackData;

    CurrentAttackData.DamageType = AttackInfo.DamageType;
    CurrentAttackData.FinalDamage = OwnerWeapon->GetCalculatedDamage() * AttackInfo.DamageMultiplier;
    CurrentAttackData.PoiseDamage = AttackInfo.PoiseDamage;
    CurrentAttackData.bUnparriable = AttackInfo.bUnparriable;

    //다단히트 여부 캐싱 - FAttackStats에 다단히트 필드가 없으므로
    //AttackTraceComponent(PerformSlashTrace가 ValidateHit에 false 전달)와 동일하게 false 유지
    bCurrentIsMultiHit = false;

    // UpdateCapsuleSize 호출 제거 - 고정 크기 유지

    return true;
}

void UCapsuleOverlapComponent::UpdateCapsuleSize(EAttackDamageType DamageType)
{
    switch (DamageType)
    {
    case EAttackDamageType::Slash:
        //베기: 길고 얇은 캡슐
        SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight * 1.2f);
        break;
        
    case EAttackDamageType::Pierce:
        //찌르기: 작고 집중된 캡슐
        SetCapsuleSize(DefaultCapsuleRadius * 0.6f, DefaultCapsuleHalfHeight * 0.8f);
        break;
        
    case EAttackDamageType::Strike:
        //타격: 크고 둔탁한 캡슐
        SetCapsuleSize(DefaultCapsuleRadius * 1.5f, DefaultCapsuleHalfHeight);
        break;
        
    default:
        SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
        break;
    }
    
    DEBUG_LOG(TEXT("Capsule size updated for %s attack"), 
              *UEnum::GetValueAsString(DamageType));
}
#pragma endregion

void UCapsuleOverlapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindEventCallbacks();
    Super::EndPlay(EndPlayReason);
}

#pragma region "Debug And Profiling"
void UCapsuleOverlapComponent::DrawDebugCapsuleShape()
{
    if (!bDrawDebugCapsule) return;

    //현재 캡슐 1개만 그림 (보간 궤적 제거)
    DrawDebugCapsule(GetWorld(),
                    GetComponentLocation(),
                    GetScaledCapsuleHalfHeight(),
                    GetScaledCapsuleRadius(),
                    GetComponentQuat(),
                    DebugDrawColor,
                    false,
                    DebugDrawDuration);
}

#pragma endregion