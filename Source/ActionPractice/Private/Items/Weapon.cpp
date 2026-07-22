#include "Public/Items/Weapon.h"
#include "Public/Items/WeaponDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Math/UnrealMathUtility.h"
#include "GameplayTagContainer.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/HitDetection/WeaponAttackComponent.h"
#include "Characters/HitDetection/WeaponCCDComponent.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"
#include "Net/UnrealNetwork.h"

const FName AWeapon::GripSocketName = TEXT("grip_oh_socket");

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogWeapon, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogWeapon, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

AWeapon::AWeapon()
{
    PrimaryActorTick.bCanEverTick = true;

	//네트워크 복제 활성화
	bReplicates = true;
	SetReplicateMovement(false);  // Attachment로 위치 동기화되므로 Movement는 복제 불필요

	// Scene Component를 Root로 설정
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

    // 메시 컴포넌트 생성
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);

    // 콜리전 컴포넌트 추가
    AttackTraceComponent = CreateDefaultSubobject<UWeaponAttackComponent>(TEXT("TraceComponent"));
    CCDComponent = CreateDefaultSubobject<UWeaponCCDComponent>(TEXT("CCDComponent"));

    // 기본 콜리전 설정
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AWeapon::BeginPlay()
{
    //무기 몽타주 프리로드를 게터 내부의 lazy 로드에서 장착(BeginPlay) 시점으로 이동
    //WeaponData는 BP 디폴트 값이라 복제 대기 없이 즉시 유효하며, 서버/클라 모두 BeginPlay가 호출되어
    //데디케이티드 서버에서도 몽타주가 준비된다 (OwnerCharacter 유효성과 무관하게 선행 실행)
    if (WeaponData)
    {
        WeaponData->PreloadAllMontages();
    }

    OwnerCharacter = Cast<AActionPracticeCharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        DEBUG_LOG(TEXT("No Owner Character In Weapon"));
        return;
    }

	CalculateCalculatedDamage();
	BindDelegates();

    Super::BeginPlay();
}


void AWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

EWeaponEnums AWeapon::GetWeaponType() const
{
    if (!WeaponData) return EWeaponEnums::None;

    return WeaponData->WeaponType;    
}

const FBlockActionData* AWeapon::GetWeaponBlockData() const
{
    if (!WeaponData) return nullptr;

    //프리로드는 BeginPlay(장착 시점)에서 선행 수행됨 - 게터 내부 lazy 로드 제거
    return &WeaponData->BlockData;
}


const FTaggedAttackData* AWeapon::GetWeaponAttackDataByTag(const FGameplayTagContainer& AttackTags) const
{
    if (!WeaponData) return nullptr;

    //프리로드는 BeginPlay(장착 시점)에서 선행 수행됨 - 게터 내부 lazy 로드 제거
    // 정확한 매칭: 전달받은 태그 컨테이너와 정확히 일치하는 키를 찾음
    for (const FTaggedAttackData& TaggedData : WeaponData->TaggedAttackData)
    {
        if (TaggedData.AttackTags == AttackTags)
        {
            return &TaggedData;
        }
    }

    return nullptr;
}

TScriptInterface<IHitDetectionInterface> AWeapon::GetHitDetectionComponent() const
{
    if (bIsTraceDetectionOrNot) return AttackTraceComponent;
    return CCDComponent;
}


void AWeapon::CalculateCalculatedDamage()
{
	if (!WeaponData)
	{
		DEBUG_LOG(TEXT("WeaponData is null"));
		return;
	}

	if (!OwnerCharacter)
	{
		DEBUG_LOG(TEXT("OwnerCharacter is null"));
		return;
	}

	UActionPracticeAttributeSet* AttributeSet = OwnerCharacter->GetAttributeSet();
	if (!AttributeSet)
	{
		DEBUG_LOG(TEXT("APAttributeSet is null"));
		return;
	}

	const float Strength = AttributeSet->GetStrength();
	const float Dexterity = AttributeSet->GetDexterity();

	const float StrengthScaling = WeaponData->StrengthScaling;
	const float DexterityScaling = WeaponData->DexterityScaling;

	//최종 대미지 계산: BaseDamage + (근력 * 근력 보정 * 0.01) + (기량 * 기량 보정 * 0.01)
	const float StrengthBonus = Strength * StrengthScaling * 0.01f;
	const float DexterityBonus = Dexterity * DexterityScaling * 0.01f;

	CalculatedDamage = WeaponData->BaseDamage + StrengthBonus + DexterityBonus;

	DEBUG_LOG(TEXT("Calculated Damage: %.2f (Base: %.2f, Str Bonus: %.2f, Dex Bonus: %.2f)"),
		CalculatedDamage, WeaponData->BaseDamage, StrengthBonus, DexterityBonus);
}

void AWeapon::AttachToCharacterHandByGripSocket(USkeletalMeshComponent* CharacterMesh, const FName HandSocketName)
{
	if (!CharacterMesh)
	{
		DEBUG_LOG(TEXT("AttachToCharacterHandByGripSocket: CharacterMesh is null"));
		return;
	}

	if (!WeaponMesh)
	{
		DEBUG_LOG(TEXT("AttachToCharacterHandByGripSocket: WeaponMesh is null"));
		return;
	}

	//손 소켓에 액터 부착
	AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocketName);

	//grip_oh_socket 존재 여부 확인
	if (!WeaponMesh->DoesSocketExist(GripSocketName))
	{
		DEBUG_LOG(TEXT("AttachToCharacterHandByGripSocket: grip_oh_socket not found on [%s]. Keeping current attachment to [%s]"),
			*GetNameSafe(this), *HandSocketName.ToString());
		return;
	}

	//grip_oh_socket의 컴포넌트 공간 트랜스폼 취득
	//RTS_Component: WeaponMesh 로컬 공간 기준 소켓 오프셋
	const FTransform GripSocketTransform = WeaponMesh->GetSocketTransform(GripSocketName, RTS_Component);

	//WeaponMeshRelative = Inverse(GripSocketTransform)
	//장착 후 grip_oh_socket 위치/회전이 손 소켓과 정확히 일치
	const FTransform NewRelativeTransform = GripSocketTransform.Inverse();

	//스케일은 BP 값 유지
	const FVector CurrentScale = WeaponMesh->GetRelativeScale3D();
	WeaponMesh->SetRelativeTransform(NewRelativeTransform);
	WeaponMesh->SetRelativeScale3D(CurrentScale);

	DEBUG_LOG(TEXT("AttachToCharacterHandByGripSocket: Attached to [%s] with grip_oh_socket alignment applied"),
		*HandSocketName.ToString());
}

void AWeapon::EquipWeapon()
{
    //무기 장착시 실행할 몽타주, 이펙트, 사운드 등의 로직
}


void AWeapon::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this)
    {
        DEBUG_LOG(TEXT("Weapon %s hit %s"), *WeaponName, *OtherActor->GetName());
        //이펙트, 사운드 등 히트 로직 추가
    }
}

void AWeapon::HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, FFinalAttackData FinalAttackData)
{
	//서버에서만 데미지 적용 (싱글플레이어에서는 항상 true)
	if (!HasAuthority())
	{
		return;
	}

    //OnHit();
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// CalculatedDamage 복제 (서버에서 계산)
	DOREPLIFETIME(AWeapon, CalculatedDamage);
}

void AWeapon::OnStrengthChanged(const FOnAttributeChangeData& Data)
{
	CalculateCalculatedDamage();
}

void AWeapon::OnDexterityChanged(const FOnAttributeChangeData& Data)
{
	CalculateCalculatedDamage();
}

void AWeapon::BindDelegates()
{
	UnbindDelegates();
	
	// HitDetection 컴포넌트 Hit 델리게이트 바인딩
	if (AttackTraceComponent)
	{
		AttackTraceHitHandle = AttackTraceComponent->OnHit.AddUObject(this, &AWeapon::HandleWeaponHit);
	}

	if (CCDComponent)
	{
		CCDHitHandle = CCDComponent->OnWeaponHit.AddUObject(this, &AWeapon::HandleWeaponHit);
	}
	
	if (!OwnerCharacter)
	{
		DEBUG_LOG(TEXT("BindDelegates: No Owner Character"));
		return;
	}

	UActionPracticeAttributeSet* AttributeSet = OwnerCharacter->GetAttributeSet();
	if (!AttributeSet)
	{
		DEBUG_LOG(TEXT("BindDelegates: No AttributeSet"));
		return;
	}

	UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent();
	if (!ASC)
	{
		DEBUG_LOG(TEXT("BindDelegates: No ASC"));
		return;
	}

	//어트리뷰트 델리게이트 등록
	PlayerStrengthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetStrengthAttribute()).AddUObject(this, &AWeapon::OnStrengthChanged);
	PlayerDexterityChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetDexterityAttribute()).AddUObject(this, &AWeapon::OnDexterityChanged);

	DEBUG_LOG(TEXT("BindDelegates: Successfully bound all delegates"));
}

void AWeapon::UnbindDelegates()
{
	//Hit 컴포넌트 델리게이트 해제
	if (AttackTraceHitHandle.IsValid() && AttackTraceComponent)
	{
		AttackTraceComponent->OnHit.Remove(AttackTraceHitHandle);
		AttackTraceHitHandle.Reset();
	}

	if (CCDHitHandle.IsValid() && CCDComponent)
	{
		CCDComponent->OnWeaponHit.Remove(CCDHitHandle);
		CCDHitHandle.Reset();
	}
	
	if (!OwnerCharacter)
	{
		return;
	}

	UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	UActionPracticeAttributeSet* AttributeSet = OwnerCharacter->GetAttributeSet();
	if (!AttributeSet)
	{
		return;
	}

	//어트리뷰트 델리게이트 해제
	if (PlayerStrengthChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			AttributeSet->GetStrengthAttribute()).Remove(PlayerStrengthChangedHandle);
		PlayerStrengthChangedHandle.Reset();
	}

	if (PlayerDexterityChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			AttributeSet->GetDexterityAttribute()).Remove(PlayerDexterityChangedHandle);
		PlayerDexterityChangedHandle.Reset();
	}

	DEBUG_LOG(TEXT("UnbindDelegates: Successfully unbound all delegates"));
}

void AWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindDelegates();
	
	Super::EndPlay(EndPlayReason);
}
