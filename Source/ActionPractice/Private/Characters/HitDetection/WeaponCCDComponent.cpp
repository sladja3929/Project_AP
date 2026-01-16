#include "Characters/HitDetection/WeaponCCDComponent.h"
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
	DEFINE_LOG_CATEGORY_STATIC(LogWeaponCCDComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWeaponCCDComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UWeaponCCDComponent::UWeaponCCDComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    
    SetUseCCD(true);
    
    //오버랩 이벤트
    OnComponentBeginOverlap.AddDynamic(this, &UWeaponCCDComponent::OnCapsuleBeginOverlap);
}

void UWeaponCCDComponent::BeginPlay()
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCollisionObjectType(ECC_GameTraceChannel1); //WeaponTrace
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    
    
    OwnerWeapon = Cast<AWeapon>(GetOwner());
    if (!OwnerWeapon)
    {
        DEBUG_LOG(TEXT("WeaponCCDComponent: Owner is not a weapon!"));
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
    
    DEBUG_LOG(TEXT("WeaponCCD initialized - Radius: %.2f (fixed), HalfHeight: %.2f"), 
              DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
    
    Super::BeginPlay();
}

void UWeaponCCDComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bIsDetecting && bDrawDebugCapsule)
    {
        DrawDebugCCDTrajectory();
    }
    
    //위치 업데이트 (디버그용)
    PreviousCapsuleLocation = GetComponentLocation();
    PreviousCapsuleRotation = GetComponentQuat();
}

#pragma region "HitDetectionInterface Implementation"
void UWeaponCCDComponent::PrepareHitDetection(const FGameplayTagContainer& AttackTags, const int32 ComboIndex)
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

void UWeaponCCDComponent::HandleHitDetectionStart(const FGameplayEventData& Payload)
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
    
    //초기 위치 저장
    PreviousCapsuleLocation = GetComponentLocation();
    PreviousCapsuleRotation = GetComponentQuat();
    
    DEBUG_LOG(TEXT("HitDetection Started - CCD Active"));
}

void UWeaponCCDComponent::HandleHitDetectionEnd(const FGameplayEventData& Payload)
{
    //충돌 비활성화
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    bIsDetecting = false;
    bIsPrepared = false;
    SetComponentTickEnabled(false);
    
    UnbindEventCallbacks();
}
#pragma endregion

#pragma region "Event Binding"
void UWeaponCCDComponent::BindEventCallbacks()
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

void UWeaponCCDComponent::UnbindEventCallbacks()
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
void UWeaponCCDComponent::OnCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                AActor* OtherActor,
                                                UPrimitiveComponent* OtherComp,
                                                int32 OtherBodyIndex,
                                                bool bFromSweep,
                                                const FHitResult& SweepResult)
{
	// 히트 판정은 서버에서만 (싱글플레이어에서는 항상 true)
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

    if (!bIsDetecting || !OtherActor) return;

    if (ValidateHit(OtherActor))
    {
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
        
        ProcessHit(OtherActor, HitResult);
    }
}

bool UWeaponCCDComponent::ValidateHit(AActor* HitActor)
{
    if (!HitActor || !OwnerWeapon) return false;
    
    //자기 자신과 소유자 제외
    AActionPracticeCharacter* WeaponOwner = OwnerWeapon->GetOwnerCharacter();
    if (HitActor == OwnerWeapon || HitActor == WeaponOwner) return false;
    
    //중복 히트 체크
    float CurrentTime = GetWorld()->GetTimeSeconds();
    for (const FHitRecord& Record : HitRecords)
    {
        if (Record.HitActor == HitActor && 
            CurrentTime - Record.HitTime < HitCooldownTime)
        {
            return false;
        }
    }
    
    return true;
}

void UWeaponCCDComponent::ProcessHit(AActor* HitActor, const FHitResult& HitResult)
{
    //히트 기록
    FHitRecord NewRecord;
    NewRecord.HitActor = HitActor;
    NewRecord.HitTime = GetWorld()->GetTimeSeconds();
    HitRecords.Add(NewRecord);
    
    DEBUG_LOG(TEXT("CCD Hit: %s at %s"), 
              *HitActor->GetName(), 
              *HitResult.Location.ToString());
    
    //이벤트 브로드캐스트
    OnWeaponHit.Broadcast(HitActor, HitResult, CurrentAttackData);
}

void UWeaponCCDComponent::ResetHitActors()
{
    HitRecords.Empty();
}
#pragma endregion

#pragma region "Configuration"
bool UWeaponCCDComponent::LoadAttackConfig(const FGameplayTagContainer& AttackTags, int32 ComboIndex)
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

    // UpdateCapsuleSize 호출 제거 - 고정 크기 유지

    return true;
}

void UWeaponCCDComponent::UpdateCapsuleSize(EAttackDamageType DamageType)
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

void UWeaponCCDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindEventCallbacks();
    Super::EndPlay(EndPlayReason);
}

#pragma region "Debug And Profiling"
void UWeaponCCDComponent::DrawDebugCCDTrajectory()
{
    if (!bDrawDebugCapsule) return;
    
    FVector CurrentLocation = GetComponentLocation();
    FQuat CurrentRotation = GetComponentQuat();
    
    //현재 캡슐 그리기
    DrawDebugCapsule(GetWorld(), 
                    CurrentLocation,
                    GetScaledCapsuleHalfHeight(),
                    GetScaledCapsuleRadius(),
                    CurrentRotation,
                    DebugCCDColor,
                    false,
                    DebugCCDDuration);
    
    //CCD 궤적 (이전 위치에서 현재 위치까지)
    if (!PreviousCapsuleLocation.IsZero())
    {
        //보간된 중간 위치들 표시
        int32 NumInterpolations = 5;
        for (int32 i = 1; i <= NumInterpolations; ++i)
        {
            float Alpha = (float)i / (float)NumInterpolations;
            FVector InterpLocation = FMath::Lerp(PreviousCapsuleLocation, CurrentLocation, Alpha);
            FQuat InterpRotation = FQuat::Slerp(PreviousCapsuleRotation, CurrentRotation, Alpha);
            
            DrawDebugCapsule(GetWorld(),
                           InterpLocation,
                           GetScaledCapsuleHalfHeight(),
                           GetScaledCapsuleRadius(),
                           InterpRotation,
                           FColor(DebugCCDColor.R, DebugCCDColor.G, DebugCCDColor.B, 50), //반투명
                           false,
                           DebugCCDDuration);
        }
        
        //궤적 선
        DrawDebugLine(GetWorld(),
                     PreviousCapsuleLocation,
                     CurrentLocation,
                     FColor::Yellow,
                     false,
                     DebugCCDDuration,
                     0,
                     2.0f);
    }
}

#pragma endregion