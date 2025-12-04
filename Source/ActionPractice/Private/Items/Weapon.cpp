#include "Public/Items/Weapon.h"
#include "Public/Items/WeaponDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Math/UnrealMathUtility.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Characters/HitDetection/WeaponAttackComponent.h"
#include "Characters/HitDetection/WeaponCCDComponent.h"
#include "GAS/AttributeSet/ActionPracticeAttributeSet.h"

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

    // 첫 번째 몽타주를 체크해서, 로드가 안되었으면 로드
    const TSoftObjectPtr<UAnimMontage>& FirstMontage = WeaponData->BlockData.BlockIdleMontage;
    if (!FirstMontage.IsNull() && !FirstMontage.IsValid()) WeaponData->PreloadAllMontages();
    
    return &WeaponData->BlockData;
}


const FTaggedAttackData* AWeapon::GetWeaponAttackDataByTag(const FGameplayTagContainer& AttackTags) const
{
    if (!WeaponData) return nullptr;

    // 첫 번째 몽타주를 체크해서, 로드가 안되었으면 로드
    for (const FTaggedAttackData& TaggedData : WeaponData->TaggedAttackData)
    {
        if (TaggedData.ComboSequence.Num() > 0)
        {
            const TSoftObjectPtr<UAnimMontage>& FirstMontage = TaggedData.ComboSequence[0].AttackMontage;
            if (!FirstMontage.IsNull() && !FirstMontage.IsValid())
            {
                WeaponData->PreloadAllMontages();
                break;
            }
        }
    }

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
    //OnHit();
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
