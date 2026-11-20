#include "AI/EnemyPackSubsystem.h"
#include "AI/EnemyPack.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyWanderArea.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Enemy/EnemyDataAsset.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyPackSubsystem, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyPackSubsystem, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool UEnemyPackSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEnemyPackSubsystem::RallyPack(AEnemyAIController* Instigator, AActor* Target)
{
	UWorld* World = GetWorld();

	if (!World || World->GetNetMode() == NM_Client || !Instigator || !Target || Instigator->GetPack())
	{
		return;
	}

	const AEnemyCharacter* Enemy = Instigator->GetEnemyCharacter();

	if (!UsesPack(Enemy))
	{
		return;
	}

	const FEnemyPackSettings& Settings = Enemy->GetEnemyData()->PackSettings;

	UEnemyPack* Pack = FindJoinablePack(Enemy, Target);

	if (!Pack)
	{
		Pack = NewObject<UEnemyPack>(this);
		Pack->Initialize(Target, Enemy->GetWanderArea(), Enemy->EnemyName, Settings);
		Packs.Add(Pack);

		if (!World->GetTimerManager().IsTimerActive(AttackerUpdateTimer))
		{
			World->GetTimerManager().SetTimer(AttackerUpdateTimer, this, &UEnemyPackSubsystem::UpdatePacks, AttackerUpdateInterval, true);
		}

		DEBUG_LOG(TEXT("RallyPack: New pack by %s (Area=%s, Target=%s)"), *GetNameSafe(Enemy), *GetNameSafe(Enemy->GetWanderArea()), *GetNameSafe(Target));
	}

	JoinPack(Pack, Instigator);

	TArray<AEnemyAIController*> Candidates;
	GatherRallyCandidates(Enemy, Settings.RallyRadius, Candidates);

	for (AEnemyAIController* Candidate : Candidates)
	{
		//전투 진입 전에 소속을 먼저 정해 EnterCombat 안의 RallyPack 재호출이 무시되게 함
		JoinPack(Pack, Candidate);
		Candidate->EnterCombat(Target);

		if (!Candidate->IsInCombat())
		{
			LeavePack(Candidate);
		}
	}

	Pack->UpdateAttackers();
}

void UEnemyPackSubsystem::LeavePack(AEnemyAIController* Member)
{
	UEnemyPack* Pack = Member ? Member->GetPack() : nullptr;

	if (!Pack)
	{
		return;
	}

	Member->SetPack(nullptr);
	Pack->RemoveMember(Member);

	DEBUG_LOG(TEXT("LeavePack: %s (Remaining=%d)"), *GetNameSafe(Member), Pack->GetMemberCount());

	if (Pack->IsEmpty())
	{
		RemovePack(Pack);
		return;
	}

	//빠진 공격자 자리 즉시 보충
	Pack->UpdateAttackers();
}

UEnemyPack* UEnemyPackSubsystem::FindJoinablePack(const AEnemyCharacter* Enemy, const AActor* Target) const
{
	const AEnemyWanderArea* Area = Enemy->GetWanderArea();

	for (UEnemyPack* Pack : Packs)
	{
		if (!Pack || Pack->GetTarget() != Target)
		{
			continue;
		}

		if (Area)
		{
			if (Pack->GetArea() == Area)
			{
				return Pack;
			}

			continue;
		}

		if (!Pack->GetArea() && Pack->GetEnemyName() == Enemy->EnemyName && Pack->HasMemberWithin(Enemy->GetActorLocation(), Pack->GetSettings().RallyRadius))
		{
			return Pack;
		}
	}

	return nullptr;
}

void UEnemyPackSubsystem::GatherRallyCandidates(const AEnemyCharacter* Instigator, float RallyRadius, TArray<AEnemyAIController*>& OutCandidates) const
{
	const AEnemyWanderArea* Area = Instigator->GetWanderArea();
	const FVector Origin = Instigator->GetActorLocation();
	const float RallyRadiusSq = FMath::Square(RallyRadius);

	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AEnemyCharacter* Candidate = *It;

		if (Candidate == Instigator || !UsesPack(Candidate))
		{
			continue;
		}

		//구역 무리는 같은 구역 전원, 구역 없는 적은 같은 종류의 구역 없는 적 중 반경 안
		if (Area)
		{
			if (Candidate->GetWanderArea() != Area)
			{
				continue;
			}
		}
		else if (Candidate->GetWanderArea() || Candidate->EnemyName != Instigator->EnemyName || FVector::DistSquared2D(Candidate->GetActorLocation(), Origin) > RallyRadiusSq)
		{
			continue;
		}

		AEnemyAIController* Controller = Candidate->GetEnemyAIController();

		if (!Controller || Controller->IsInCombat() || Controller->GetPack())
		{
			continue;
		}

		const UBaseAbilitySystemComponent* ASC = Cast<UBaseAbilitySystemComponent>(Candidate->GetAbilitySystemComponent());

		if (ASC && ASC->IsDeathHandled())
		{
			continue;
		}

		OutCandidates.Add(Controller);
	}
}

void UEnemyPackSubsystem::JoinPack(UEnemyPack* Pack, AEnemyAIController* Member)
{
	Pack->AddMember(Member);
	Member->SetPack(Pack);
}

void UEnemyPackSubsystem::RemovePack(UEnemyPack* Pack)
{
	Packs.Remove(Pack);

	if (Packs.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttackerUpdateTimer);
		}
	}
}

void UEnemyPackSubsystem::UpdatePacks()
{
	//RemovePack이 배열을 바꾸므로 복사본 순회
	const TArray<TObjectPtr<UEnemyPack>> PacksCopy = Packs;

	for (UEnemyPack* Pack : PacksCopy)
	{
		if (!Pack)
		{
			continue;
		}

		Pack->PruneInvalidMembers();

		if (Pack->IsEmpty())
		{
			RemovePack(Pack);
			continue;
		}

		Pack->UpdateAttackers();
	}
}

bool UEnemyPackSubsystem::UsesPack(const AEnemyCharacter* Enemy)
{
	const UEnemyDataAsset* EnemyData = Enemy ? Enemy->GetEnemyData() : nullptr;
	return EnemyData && EnemyData->PackSettings.bEnablePack;
}

void UEnemyPackSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackerUpdateTimer);
	}

	//월드 정리 중 늦게 오는 컨트롤러 탈퇴(OnUnPossess 등)가 해제된 무리를 건드리지 않도록 소속을 먼저 끊음
	for (UEnemyPack* Pack : Packs)
	{
		if (!Pack)
		{
			continue;
		}

		for (const TWeakObjectPtr<AEnemyAIController>& Member : Pack->GetMembers())
		{
			if (Member.IsValid())
			{
				Member->SetPack(nullptr);
			}
		}
	}

	Packs.Reset();

	Super::Deinitialize();
}
