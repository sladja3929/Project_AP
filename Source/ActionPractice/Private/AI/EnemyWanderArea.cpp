#include "AI/EnemyWanderArea.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyWanderArea, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyWanderArea, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

AEnemyWanderArea::AEnemyWanderArea()
{
	PrimaryActorTick.bCanEverTick = false;

	AreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBox"));
	SetRootComponent(AreaBox);

	//구역 표시 전용 (충돌·오버랩 없음, 게임에서는 숨김)
	AreaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaBox->SetGenerateOverlapEvents(false);
	AreaBox->SetCanEverAffectNavigation(false);
	AreaBox->SetBoxExtent(FVector(800.0f, 800.0f, 200.0f));
	AreaBox->ShapeColor = FColor(80, 200, 120);
	AreaBox->SetLineThickness(4.0f);
	AreaBox->SetHiddenInGame(true);
}

bool AEnemyWanderArea::IsInside(const FVector& Location) const
{
	const FTransform& BoxTransform = AreaBox->GetComponentTransform();
	const FVector Local = BoxTransform.InverseTransformPositionNoScale(Location);
	const FVector Extent = AreaBox->GetScaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y;
}

bool AEnemyWanderArea::IsTooCloseToOthers(const APawn* Requester, const FVector& Location) const
{
	const float MinSeparationSq = FMath::Square(MinSeparation);

	for (const TPair<TWeakObjectPtr<const APawn>, FVector>& Pair : ReservedPoints)
	{
		const APawn* Member = Pair.Key.Get();

		if (!Member || Member == Requester)
		{
			continue;
		}

		if (FVector::DistSquared2D(Pair.Value, Location) < MinSeparationSq)
		{
			return true;
		}
	}

	return false;
}

void AEnemyWanderArea::ReservePoint(const APawn* Requester, const FVector& Location)
{
	if (!Requester)
	{
		return;
	}

	//사라진 구성원 정리
	for (auto It = ReservedPoints.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	ReservedPoints.Add(Requester, Location);
}

void AEnemyWanderArea::ReleasePoint(const APawn* Requester)
{
	ReservedPoints.Remove(Requester);
}

FVector AEnemyWanderArea::GetRandomPointInside() const
{
	//박스 로컬 X·Y 범위에서 뽑아 월드로 변환 (외접원 샘플링은 길쭉한 박스에서 대부분 탈락)
	const FVector Extent = AreaBox->GetScaledBoxExtent();
	const FVector Local(FMath::FRandRange(-Extent.X, Extent.X), FMath::FRandRange(-Extent.Y, Extent.Y), 0.0f);

	return AreaBox->GetComponentTransform().TransformPositionNoScale(Local);
}

FVector AEnemyWanderArea::GetAreaCenter() const
{
	return AreaBox->GetComponentLocation();
}
