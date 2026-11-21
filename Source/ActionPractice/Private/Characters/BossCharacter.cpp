#include "Characters/BossCharacter.h"

#include "Characters/ActionPracticeCharacter.h"
#include "AI/BossArena.h"
#include "AI/EnemyAIController.h"
#include "Components/WidgetComponent.h"
#include "Games/ActionPracticePlayerController.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossCharacter, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBossCharacter, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ABossCharacter::ABossCharacter()
{
	//부모(EnemyCharacter)가 CapsuleSize, CMC, ASC, AttributeSet, EnemyAttackComponent 등 모두 처리
	//보스 전용 추가 초기화가 필요하면 여기에 작성
}

void ABossCharacter::BeginPlay()
{
	//EnemyCharacter::BeginPlay에서 EnemyData 프리로드, 전투 상태 바인딩, CacheInitialEnemyState 모두 처리
	Super::BeginPlay();

	//보스는 머리 위 HP바를 사용하지 않음
	if (EnemyHealthBarWidgetComponent)
	{
		EnemyHealthBarWidgetComponent->SetVisibility(false);
		EnemyHealthBarWidgetComponent->SetHiddenInGame(true);
	}

	//보스룸 진입 인지 (AI는 서버 전용)
	if (HasAuthority() && BossArena)
	{
		BossArena->OnPlayerEntered.AddUObject(this, &ABossCharacter::OnPlayerEnteredArena);
		PerceivePlayersInArena();
	}
}

void ABossCharacter::OnPlayerEnteredArena(AActor* Player)
{
	if (AEnemyAIController* AIController = GetEnemyAIController())
	{
		AIController->PerceiveTarget(Player);
	}
}

bool ABossCharacter::IsTargetInCombatArea(const AActor* Target) const
{
	return BossArena && BossArena->IsActorInside(Target);
}

void ABossCharacter::PerceivePlayersInArena()
{
	if (!HasAuthority() || !BossArena)
	{
		return;
	}

	TArray<AActor*> PlayersInArena;
	BossArena->GetOverlappingPlayers(PlayersInArena);

	//PerceiveTarget이 첫 유효 플레이어만 받고 이후는 무시
	for (AActor* Player : PlayersInArena)
	{
		OnPlayerEnteredArena(Player);
	}
}

void ABossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BossArena)
	{
		BossArena->OnPlayerEntered.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ABossCharacter::ShowEnemyHealthBar()
{
	//보스는 머리 위 HP바 사용 안 함 — 의도적 빈 구현
}

void ABossCharacter::HideEnemyHealthBar()
{
	//보스는 머리 위 HP바 사용 안 함 — 의도적 빈 구현
}

void ABossCharacter::ResetEnemy()
{
	//공통 리셋 로직 (어빌리티 취소, 태그 제거, 애니메이션, 이동, 텔레포트, GE, AI 재시작)
	Super::ResetEnemy();

	//보스 전용 리셋
	if (bBossEncountered)
	{
		bBossEncountered = false;
		Multicast_OnBossDisengage();
	}

	//리셋으로 인지가 지워진 뒤 보스룸에 남아 있는 플레이어 재인지 (퇴장·재진입 없이도)
	PerceivePlayersInArena();

	DEBUG_LOG(TEXT("BossCharacter::ResetEnemy: Boss-specific reset complete"));
}

void ABossCharacter::OnCombatStateChanged(bool bInCombat, AActor* Target)
{
	Super::OnCombatStateChanged(bInCombat, Target);

	if (bInCombat)
	{
		//보스가 이미 사망한 경우 조우 연출 재실행 방지
		UBaseAbilitySystemComponent* BaseASC = Cast<UBaseAbilitySystemComponent>(AbilitySystemComponent);
		if (BaseASC && BaseASC->IsDeathHandled()) return;

		if (!bBossEncountered)
		{
			bBossEncountered = true;

			//서버에서 모든 클라이언트에 조우 연출 전파
			Multicast_OnBossEncounter();
		}

		return;
	}

	if (bBossEncountered)
	{
		bBossEncountered = false;

		//서버에서 모든 클라이언트에 이탈 연출 전파
		Multicast_OnBossDisengage();
	}
}

void ABossCharacter::PlayBossBGM()
{
	if (!BossBGM)
	{
		DEBUG_LOG(TEXT("BossBGM is not set"));
		return;
	}

	//이미 재생 중이면 리턴
	if (BGMAudioComponent && BGMAudioComponent->IsPlaying())
	{
		DEBUG_LOG(TEXT("BossBGM is already playing"));
		return;
	}

	BGMAudioComponent = UGameplayStatics::SpawnSound2D(this, BossBGM);
	if (BGMAudioComponent)
	{
		DEBUG_LOG(TEXT("BossBGM started playing"));
	}
}

void ABossCharacter::StopBossBGM()
{
	if (BGMAudioComponent && BGMAudioComponent->IsPlaying())
	{
		BGMAudioComponent->Stop();
		DEBUG_LOG(TEXT("BossBGM stopped"));
	}
}

void ABossCharacter::Multicast_OnBossEncounter_Implementation()
{
	DEBUG_LOG(TEXT("Multicast_OnBossEncounter called"));

	//로컬 PlayerController에 보스 HP바 표시 요청
	AActionPracticePlayerController* PC = Cast<AActionPracticePlayerController>(
		UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC)
	{
		PC->ShowBossHealth(this);
	}

	PlayBossBGM();
}

void ABossCharacter::Multicast_OnBossDisengage_Implementation()
{
	DEBUG_LOG(TEXT("Multicast_OnBossDisengage called"));

	//로컬 PlayerController에 보스 HP바 숨김 요청
	AActionPracticePlayerController* PC = Cast<AActionPracticePlayerController>(
		UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC)
	{
		PC->HideBossHealth();
	}

	StopBossBGM();
}
