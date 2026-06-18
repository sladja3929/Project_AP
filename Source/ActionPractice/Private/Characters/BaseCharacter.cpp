#include "Characters/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/AbilitySet/AbilitySetDataAsset.h"
#include "GAS/CharacterStats/CharacterStatsDataAsset.h"
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

	//네트워크 복제 활성화
	bReplicates = true;
	SetReplicateMovement(true);

	//데디케이티드 서버에서도 애니메이션 틱 활성화 (몽타주 콜백을 위해 필수)
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	//GAS 초기화는 possess 타이밍으로 이동 (ActionPracticeCharacter::PossessedBy / OnRep_Owner)
	//단, possess가 없는 케이스(적 캐릭터 등)를 위해 자식에서 필요 시 직접 호출
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

	//AbilitySystemComponent와 AttributeSet은 GAS에서 자동 복사
}

void ABaseCharacter::InitializeAbilitySystem()
{
	//중복 초기화 방지 (PossessedBy/OnRep_Owner 양쪽에서 호출될 수 있음)
	if (bAbilitySystemInitialized) return;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		//어트리뷰트 초기화를 먼저 수행 (어빌리티보다 선행해야 함)
		ApplyInitialAttributes();

		GrantStartupAbilitySets();

		bAbilitySystemInitialized = true;
	}
}

void ABaseCharacter::ApplyInitialAttributes()
{
	if (!HasAuthority()) return;
	if (!AbilitySystemComponent) return;

	if (CharacterStatsData)
	{
		CharacterStatsData->ApplyInitialAttributes(AbilitySystemComponent);
	}
}

void ABaseCharacter::GrantStartupAbilitySets()
{
	//서버에서만 부여 (클라이언트에는 복제됨)
	if (!HasAuthority()) return;
	if (!AbilitySystemComponent) return;

	for (const TObjectPtr<UAbilitySetDataAsset>& AbilitySet : StartAbilitySetsData)
	{
		if (AbilitySet)
		{
			FAbilitySetGrantedHandles& Handles = GrantedSetHandles.AddDefaulted_GetRef();
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &Handles, this);
		}
	}
}

void ABaseCharacter::RemoveAllAbilitySets()
{
	if (!AbilitySystemComponent) return;

	for (FAbilitySetGrantedHandles& Handles : GrantedSetHandles)
	{
		Handles.RemoveFromASC(AbilitySystemComponent);
	}

	GrantedSetHandles.Empty();
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