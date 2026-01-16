#include "Characters/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilities/Public/Abilities/GameplayAbility.h"
#include "GAS/AttributeSet/BaseAttributeSet.h"
#include "Items/AttackData.h"
#include "Net/UnrealNetwork.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBaseCharacter, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBaseCharacter, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 네트워크 복제 활성화
	bReplicates = true;
	SetReplicateMovement(true);
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
}

void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateActionRotation(DeltaTime);
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// AbilitySystemComponent와 AttributeSet은 자체적으로 복제되므로 여기서는 등록 불필요
}

void ABaseCharacter::InitializeAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		GrantStartupAbilities();

		ApplyStartupEffects();
	}
}

void ABaseCharacter::GiveAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (AbilitySystemComponent && AbilityClass)
	{
		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
		AbilitySystemComponent->GiveAbility(AbilitySpec);
	}
}

void ABaseCharacter::GrantStartupAbilities()
{
	for (const auto& StartAbility : StartAbilities)
	{
		GiveAbility(StartAbility);
	}
}

void ABaseCharacter::ApplyStartupEffects()
{
	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	for (const auto& StartEffect : StartEffects)
	{
		if (StartEffect)
		{
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(StartEffect, 1, EffectContext);
			if (SpecHandle.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}
}

void ABaseCharacter::RotateToRotation(const FRotator& TargetRotation, float RotateTime)
{
	//목표 회전 설정
	TargetActionRotation = FRotator(0.0f, TargetRotation.Yaw, 0.0f);

	//회전 시간이 0 이하면 즉시 회전
	if (RotateTime <= 0.0f)
	{
		SetActorRotation(TargetActionRotation);
		bIsRotatingForAction = false;
		DEBUG_LOG(TEXT("RotateToRotation: Instant rotation"));
		return;
	}

	//회전 각도 차이 계산
	float YawDifference = FMath::Abs(FMath::FindDeltaAngleDegrees(
		GetActorRotation().Yaw,
		TargetActionRotation.Yaw
	));

	//회전 차이가 매우 작으면 즉시 완료
	if (YawDifference < 1.0f)
	{
		SetActorRotation(TargetActionRotation);
		bIsRotatingForAction = false;
		DEBUG_LOG(TEXT("RotateToRotation: Minor rotation"));
		return;
	}

	//스무스 회전 시작
	StartActionRotation = GetActorRotation();
	CurrentRotationTime = 0.0f;
	TotalRotationTime = RotateTime;
	bIsRotatingForAction = true;
	DEBUG_LOG(TEXT("RotateToRotation: Starting smooth rotation over %.2f seconds"), RotateTime);
}

void ABaseCharacter::RotateToPosition(const FVector& TargetLocation, float RotateTime)
{
	//타겟 방향 계산
	FVector Direction = TargetLocation - GetActorLocation();
	Direction.Z = 0.0f; //수평 회전만 수행
	Direction.Normalize();

	//FRotator로 변환 후 RotateToRotation 호출
	FRotator TargetRotation = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f);
	RotateToRotation(TargetRotation, RotateTime);
}

void ABaseCharacter::UpdateActionRotation(float DeltaTime)
{
	if (!bIsRotatingForAction)
	{
		return;
	}

	CurrentRotationTime += DeltaTime;

	float Alpha = FMath::Clamp(CurrentRotationTime / TotalRotationTime, 0.0f, 1.0f);

	//부드러운 커브 적용
	Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

	//회전 보간
	FRotator NewRotation = FMath::Lerp(StartActionRotation, TargetActionRotation, Alpha);
	SetActorRotation(NewRotation);

	if (CurrentRotationTime >= TotalRotationTime)
	{
		//정확한 목표 회전으로 설정
		SetActorRotation(TargetActionRotation);
		bIsRotatingForAction = false;
		CurrentRotationTime = 0.0f;
		DEBUG_LOG(TEXT("UpdateActionRotation: Rotation completed"));
	}
}