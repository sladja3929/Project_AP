#include "AI/BossArena.h"
#include "Components/BoxComponent.h"
#include "Characters/ActionPracticeCharacter.h"
#include "Components/CapsuleComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossArena, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBossArena, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ABossArena::ABossArena()
{
	PrimaryActorTick.bCanEverTick = false;

	ArenaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaBox"));
	SetRootComponent(ArenaBox);

	//Pawn 진입 감지 전용 (막지 않고 오버랩만, 시야·무기 트레이스와 무관)
	ArenaBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArenaBox->SetCollisionObjectType(ECC_WorldDynamic);
	ArenaBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArenaBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ArenaBox->SetGenerateOverlapEvents(true);
	ArenaBox->SetCanEverAffectNavigation(false);
	ArenaBox->SetBoxExtent(FVector(2000.0f, 2000.0f, 500.0f));
	ArenaBox->ShapeColor = FColor(220, 60, 60);
	ArenaBox->SetLineThickness(6.0f);
	ArenaBox->SetHiddenInGame(true);
}

void ABossArena::BeginPlay()
{
	Super::BeginPlay();

	//AI 인지는 서버에서만 동작
	if (HasAuthority())
	{
		ArenaBox->OnComponentBeginOverlap.AddDynamic(this, &ABossArena::OnArenaBeginOverlap);
	}
}

void ABossArena::GetOverlappingPlayers(TArray<AActor*>& OutPlayers) const
{
	ArenaBox->GetOverlappingActors(OutPlayers, AActionPracticeCharacter::StaticClass());
}

bool ABossArena::IsActorInside(const AActor* Actor) const
{
	return Actor && ArenaBox->IsOverlappingActor(Actor);
}

void ABossArena::OnArenaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//플레이어 캡슐만 알림 (메시 등 다른 Pawn 채널 컴포넌트로 인한 중복 방지. 생존·인지 가능 여부는 수신 측 AI 컨트롤러가 판정)
	const AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(OtherActor);

	if (!Player || OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	DEBUG_LOG(TEXT("Player entered arena: %s"), *GetNameSafe(OtherActor));
	OnPlayerEntered.Broadcast(OtherActor);
}
