#include "AI/Debug/GameplayDebuggerCategory_EnemyAI.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU
#include "AI/EnemyAIController.h"
#include "AI/StateTree/GASStateTreeAIComponent.h"
#include "Characters/EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "AI/EnemyWanderArea.h"
#include "AI/EnemyPack.h"
#include "Components/BoxComponent.h"
#include "Math/ColorList.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogGameplayDebuggerCategory_EnemyAI, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogGameplayDebuggerCategory_EnemyAI, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

namespace EnemyAIDebug
{
	//StateTree 파라미터 이름 → 표시 색 (안쪽 링부터)
	//적마다 쓰는 파라미터가 달라 없는 항목은 생략됨
	//색 구분: 빨강 근접 공격, 주황 원거리 공격 사용 거리, 초록 위치 유지 거리, 노랑 추격·전투 진입, 자홍 인지
	struct FRangeEntry
	{
		const TCHAR* ParameterName;
		FColor Color;
		const TCHAR* TextColor;
	};

	static const FRangeEntry RangeEntries[] =
	{
		{ TEXT("MeleeRange"), FColor::Red, TEXT("red") },
		{ TEXT("JumpAttackMinRange"), FColor::Orange, TEXT("orange") },
		{ TEXT("JumpAttackMaxRange"), FColor::Orange, TEXT("orange") },
		{ TEXT("SuperJumpMinRange"), FColor::Orange, TEXT("orange") },
		{ TEXT("StrafeMinRange"), FColor::Green, TEXT("green") },
		{ TEXT("HoldRingMin"), FColor::Green, TEXT("green") },
		{ TEXT("HoldRingMax"), FColor::Green, TEXT("green") },
		{ TEXT("ChaseRange"), FColor::Yellow, TEXT("yellow") },
		{ TEXT("CombatEntryRange"), FColor::Yellow, TEXT("yellow") },
		{ TEXT("AlertRange"), FColor::Magenta, TEXT("magenta") },
	};

	//시야 표시 색 (범위 링의 난색 계열과 구분되는 청색)
	static const FColor SightColor = FColorList::SkyBlue;

	constexpr float RingThickness = 3.0f;
	constexpr int32 ArcSegments = 24;
}

FGameplayDebuggerCategory_EnemyAI::FGameplayDebuggerCategory_EnemyAI()
{
	bShowOnlyWithDebugActor = true;
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_EnemyAI::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_EnemyAI());
}

void FGameplayDebuggerCategory_EnemyAI::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(DebugActor);

	if (!Enemy)
	{
		AddTextLine(TEXT("{red}Debug actor is not an EnemyCharacter"));
		return;
	}

	//링이 지형에 묻히지 않도록 캡슐 바닥보다 살짝 위
	const float HalfHeight = Enemy->GetCapsuleComponent() ? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const FVector Center = Enemy->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight - 5.0f);

	CollectStateAndTarget(Enemy);
	CollectStateTreeRanges(Enemy, Center);
	CollectPerception(Enemy, Center);
	CollectWanderArea(Enemy, Center);
}

#pragma region "Collect"

void FGameplayDebuggerCategory_EnemyAI::CollectStateAndTarget(const AEnemyCharacter* Enemy)
{
	const AEnemyAIController* AIController = Enemy->GetEnemyAIController();

	if (!AIController)
	{
		AddTextLine(TEXT("{red}No EnemyAIController"));
		return;
	}

	if (const UGASStateTreeAIComponent* StateTreeComponent = AIController->GetStateTreeComponent())
	{
		const TArray<FName> ActiveStates = StateTreeComponent->GetActiveStateNames();
		const FString StatePath = FString::JoinBy(ActiveStates, TEXT(" > "), [](const FName& Name) { return Name.ToString(); });
		AddTextLine(FString::Printf(TEXT("State: {white}%s"), StatePath.IsEmpty() ? TEXT("(none)") : *StatePath));
	}

	const FCurrentTarget& TargetInfo = AIController->GetCurrentTarget();
	const AActor* TargetActor = TargetInfo.IsValid() ? TargetInfo.Actor.Get() : nullptr;

	if (!TargetActor)
	{
		AddTextLine(TEXT("Awareness: {grey}None  {white}Target: {grey}none"));
		return;
	}

	AddTextLine(AIController->IsInCombat() ? TEXT("Awareness: {red}Combat") : TEXT("Awareness: {yellow}Perceived"));

	//무리 역할 (공격자 / 견제)
	if (const UEnemyPack* Pack = AIController->GetPack())
	{
		AddTextLine(FString::Printf(TEXT("Pack: %s  {grey}Members: {white}%d  {grey}Attackers: {white}%d/%d"),
			AIController->IsPackAttacker() ? TEXT("{red}Attacker") : TEXT("{cyan}Support"),
			Pack->GetMemberCount(), Pack->GetAttackerCount(), Pack->GetSettings().MaxAttackers));
	}

	//UpdateTargetInfoEvaluator와 같은 기준 (액터 중심 간 수평 거리)
	const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
	AddTextLine(FString::Printf(TEXT("Target: {white}%s  {grey}Distance: {white}%.0f"), *TargetActor->GetName(), Distance));
	AddShape(FGameplayDebuggerShape::MakeSegment(Enemy->GetActorLocation(), TargetActor->GetActorLocation(), 2.0f, FColor::White, FString::Printf(TEXT("%.0f"), Distance)));
}

void FGameplayDebuggerCategory_EnemyAI::CollectStateTreeRanges(const AEnemyCharacter* Enemy, const FVector& Center)
{
	const AEnemyAIController* AIController = Enemy->GetEnemyAIController();
	const UGASStateTreeAIComponent* StateTreeComponent = AIController ? AIController->GetStateTreeComponent() : nullptr;

	if (!StateTreeComponent)
	{
		return;
	}

	for (const EnemyAIDebug::FRangeEntry& Entry : EnemyAIDebug::RangeEntries)
	{
		float Range = 0.0f;

		//파라미터가 없는 StateTree(다른 적)는 해당 링만 생략
		if (!StateTreeComponent->GetStateTreeParameterFloat(Entry.ParameterName, Range) || Range <= 0.0f)
		{
			continue;
		}

		AddShape(FGameplayDebuggerShape::MakeCircle(Center, FVector::UpVector, Range, EnemyAIDebug::RingThickness, Entry.Color, FString::Printf(TEXT("%s %.0f"), Entry.ParameterName, Range)));
		AddTextLine(FString::Printf(TEXT("{%s}%s: {white}%.0f"), Entry.TextColor, Entry.ParameterName, Range));
	}
}

void FGameplayDebuggerCategory_EnemyAI::CollectPerception(const AEnemyCharacter* Enemy, const FVector& Center)
{
	const AEnemyAIController* AIController = Enemy->GetEnemyAIController();
	const UAIPerceptionComponent* PerceptionComponent = AIController ? AIController->GetAIPerceptionComponent() : nullptr;
	const UAISenseConfig_Sight* SightConfig = PerceptionComponent ? PerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>() : nullptr;

	if (!SightConfig)
	{
		return;
	}

	const FVector Forward = Enemy->GetActorForwardVector();
	const float HalfAngle = SightConfig->PeripheralVisionAngleDegrees;

	AddSightArc(Center, Forward, SightConfig->SightRadius, HalfAngle, EnemyAIDebug::SightColor, FString::Printf(TEXT("Sight %.0f"), SightConfig->SightRadius));
	//전투 중에는 시야각과 무관하게 LoseSightRadius 거리로만 해제되므로 전방위로 표시
	const float LoseSightHalfAngle = AIController->IsInCombat() ? 180.0f : HalfAngle;
	AddSightArc(Center, Forward, SightConfig->LoseSightRadius, LoseSightHalfAngle, FColor(128, 128, 128), FString::Printf(TEXT("LoseSight %.0f"), SightConfig->LoseSightRadius));
	AddTextLine(FString::Printf(TEXT("{skyblue}Sight: {white}%.0f  {grey}LoseSight: {white}%.0f  {grey}Peripheral: {white}%.0f"), SightConfig->SightRadius, SightConfig->LoseSightRadius, HalfAngle));
}

void FGameplayDebuggerCategory_EnemyAI::CollectWanderArea(const AEnemyCharacter* Enemy, const FVector& Center)
{
	const AEnemyWanderArea* Area = Enemy->GetWanderArea();

	if (!Area || !Area->GetAreaBox())
	{
		AddTextLine(TEXT("{green}Wander: {grey}spawn radius"));
		return;
	}

	//구역 박스를 디버그 대상 바닥 높이에 납작하게 표시
	const UBoxComponent* Box = Area->GetAreaBox();
	const FVector Extent = Box->GetScaledBoxExtent();
	const FVector BoxCenter(Box->GetComponentLocation().X, Box->GetComponentLocation().Y, Center.Z);

	AddShape(FGameplayDebuggerShape::MakeBox(BoxCenter, Box->GetComponentRotation(), FVector(Extent.X, Extent.Y, 5.0f), EnemyAIDebug::RingThickness, FColorList::LimeGreen, Area->GetActorNameOrLabel()));

	//구성원별 목적지 (디버그 대상은 강조)
	for (const TPair<TWeakObjectPtr<const APawn>, FVector>& Pair : Area->GetReservedPoints())
	{
		const APawn* Member = Pair.Key.Get();

		if (!Member)
		{
			continue;
		}

		const bool bIsDebugActor = Member == Enemy;
		AddShape(FGameplayDebuggerShape::MakePoint(Pair.Value, bIsDebugActor ? 20.0f : 12.0f, bIsDebugActor ? FColor::White : FColorList::LimeGreen, bIsDebugActor ? TEXT("Wander Target") : FString()));

		if (bIsDebugActor)
		{
			AddShape(FGameplayDebuggerShape::MakeSegment(Enemy->GetActorLocation(), Pair.Value, 1.5f, FColorList::LimeGreen));
		}
	}

	AddTextLine(FString::Printf(TEXT("{green}Wander: {white}%s  {grey}Members: {white}%d"), *Area->GetActorNameOrLabel(), Area->GetReservedPoints().Num()));
}

#pragma endregion

#pragma region "Shapes"

void FGameplayDebuggerCategory_EnemyAI::AddSightArc(const FVector& Center, const FVector& Forward, float Radius, float HalfAngleDegrees, const FColor& Color, const FString& Description)
{
	//PeripheralVisionAngleDegrees는 정면 기준 한쪽 각도 (180이면 전방위)
	const float ClampedHalfAngle = FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f);
	TArray<FVector> ArcPoints;
	ArcPoints.Reserve(EnemyAIDebug::ArcSegments + 1);

	for (int32 Index = 0; Index <= EnemyAIDebug::ArcSegments; ++Index)
	{
		const float Angle = FMath::Lerp(-ClampedHalfAngle, ClampedHalfAngle, static_cast<float>(Index) / EnemyAIDebug::ArcSegments);
		ArcPoints.Add(Center + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Radius);
	}

	AddShape(FGameplayDebuggerShape::MakePolyline(ArcPoints, EnemyAIDebug::RingThickness, Color, Description));

	//전방위가 아니면 부채꼴 경계선
	if (ClampedHalfAngle < 180.0f)
	{
		AddShape(FGameplayDebuggerShape::MakeSegment(Center, ArcPoints[0], 1.0f, Color));
		AddShape(FGameplayDebuggerShape::MakeSegment(Center, ArcPoints.Last(), 1.0f, Color));
	}
}

#pragma endregion

#endif // WITH_GAMEPLAY_DEBUGGER_MENU
