#include "AI/EnemyPack.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyWanderArea.h"
#include "GameFramework/Pawn.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyPack, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyPack, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

void UEnemyPack::Initialize(AActor* InTarget, AEnemyWanderArea* InArea, FName InEnemyName, const FEnemyPackSettings& InSettings)
{
	Target = InTarget;
	Area = InArea;
	EnemyName = InEnemyName;
	Settings = InSettings;
}

void UEnemyPack::AddMember(AEnemyAIController* Member)
{
	if (!Member)
	{
		return;
	}

	Members.AddUnique(Member);
}

void UEnemyPack::RemoveMember(const AEnemyAIController* Member)
{
	Members.RemoveAll([Member](const TWeakObjectPtr<AEnemyAIController>& Entry) { return Entry.Get() == Member; });
	Attackers.RemoveAll([Member](const TWeakObjectPtr<AEnemyAIController>& Entry) { return Entry.Get() == Member; });
}

void UEnemyPack::PruneInvalidMembers()
{
	auto IsInvalid = [](const TWeakObjectPtr<AEnemyAIController>& Entry)
	{
		return !Entry.IsValid() || !Entry->GetPawn();
	};

	Members.RemoveAll(IsInvalid);
	Attackers.RemoveAll(IsInvalid);
}

void UEnemyPack::UpdateAttackers()
{
	if (!Target.IsValid())
	{
		Attackers.Reset();
		return;
	}

	struct FCandidate
	{
		AEnemyAIController* Controller;
		float Distance;
	};

	TArray<FCandidate> Candidates;
	Candidates.Reserve(Members.Num());

	for (const TWeakObjectPtr<AEnemyAIController>& Entry : Members)
	{
		AEnemyAIController* Controller = Entry.Get();
		const float Distance = GetMemberDistance(Controller);

		if (Controller && Distance >= 0.0f && Controller->IsInCombat())
		{
			Candidates.Add({ Controller, Distance });
		}
	}

	//후보에서 빠진 공격자 제거
	Attackers.RemoveAll([&Candidates](const TWeakObjectPtr<AEnemyAIController>& Entry)
	{
		return !Candidates.ContainsByPredicate([&Entry](const FCandidate& Candidate) { return Candidate.Controller == Entry.Get(); });
	});

	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Distance < B.Distance; });

	const int32 MaxAttackers = FMath::Max(1, Settings.MaxAttackers);

	//빈 자리는 가까운 순으로 채움
	for (const FCandidate& Candidate : Candidates)
	{
		if (Attackers.Num() >= MaxAttackers)
		{
			break;
		}

		if (!IsAttacker(Candidate.Controller))
		{
			Attackers.Add(Candidate.Controller);
		}
	}

	//가장 가까운 비공격자가 가장 먼 공격자보다 여유 거리 이상 가까우면 교체 (교체마다 공격자 거리 합이 줄어 반드시 끝남)
	while (true)
	{
		const FCandidate* ClosestSupporter = Candidates.FindByPredicate([this](const FCandidate& Candidate) { return !IsAttacker(Candidate.Controller); });

		if (!ClosestSupporter)
		{
			break;
		}

		int32 FarthestIndex = INDEX_NONE;
		float FarthestDistance = -1.0f;

		for (int32 Index = 0; Index < Attackers.Num(); ++Index)
		{
			const float Distance = GetMemberDistance(Attackers[Index].Get());

			if (Distance > FarthestDistance)
			{
				FarthestDistance = Distance;
				FarthestIndex = Index;
			}
		}

		if (FarthestIndex == INDEX_NONE || ClosestSupporter->Distance + Settings.AttackerSwapMargin >= FarthestDistance)
		{
			break;
		}

		DEBUG_LOG(TEXT("UpdateAttackers: %s -> %s"), *GetNameSafe(Attackers[FarthestIndex].Get()), *GetNameSafe(ClosestSupporter->Controller));
		Attackers[FarthestIndex] = ClosestSupporter->Controller;
	}
}

bool UEnemyPack::IsAttacker(const AEnemyAIController* Member) const
{
	return Attackers.ContainsByPredicate([Member](const TWeakObjectPtr<AEnemyAIController>& Entry) { return Entry.Get() == Member; });
}

bool UEnemyPack::AreAllMembersBeyond(float Distance) const
{
	for (const TWeakObjectPtr<AEnemyAIController>& Entry : Members)
	{
		const float MemberDistance = GetMemberDistance(Entry.Get());

		if (MemberDistance >= 0.0f && MemberDistance <= Distance)
		{
			return false;
		}
	}

	return true;
}

bool UEnemyPack::HasMemberWithin(const FVector& Location, float Radius) const
{
	const float RadiusSq = FMath::Square(Radius);

	for (const TWeakObjectPtr<AEnemyAIController>& Entry : Members)
	{
		const APawn* Pawn = Entry.IsValid() ? Entry->GetPawn() : nullptr;

		if (Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), Location) <= RadiusSq)
		{
			return true;
		}
	}

	return false;
}

float UEnemyPack::GetMemberDistance(const AEnemyAIController* Member) const
{
	const APawn* Pawn = Member ? Member->GetPawn() : nullptr;
	const AActor* TargetActor = Target.Get();

	if (!Pawn || !TargetActor)
	{
		return -1.0f;
	}

	//UpdateTargetInfoEvaluator와 같은 기준 (액터 중심 간 수평 거리)
	return FVector::Dist2D(Pawn->GetActorLocation(), TargetActor->GetActorLocation());
}
