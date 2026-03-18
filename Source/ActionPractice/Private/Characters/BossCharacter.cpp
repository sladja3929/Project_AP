#include "Characters/BossCharacter.h"

#include "Characters/ActionPracticeCharacter.h"
#include "Components/WidgetComponent.h"
#include "Games/ActionPracticePlayerController.h"
#include "Perception/AISense_Sight.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

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
	//EnemyCharacter::BeginPlay에서 EnemyData 프리로드, Perception 바인딩, CacheInitialEnemyState 모두 처리
	Super::BeginPlay();

	//보스는 머리 위 HP바를 사용하지 않음
	if (EnemyHealthBarWidgetComponent)
	{
		EnemyHealthBarWidgetComponent->SetVisibility(false);
		EnemyHealthBarWidgetComponent->SetHiddenInGame(true);
	}
}

void ABossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

	DEBUG_LOG(TEXT("BossCharacter::ResetEnemy: Boss-specific reset complete"));
}

void ABossCharacter::OnPlayerDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	//시각 감지 케이스 — 보스 전용 조우 연출
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		AActionPracticeCharacter* Player = Cast<AActionPracticeCharacter>(Actor);
		if (!Player) return;

		if (Stimulus.WasSuccessfullySensed())
		{
			DEBUG_LOG(TEXT("Player detected by Boss: %s"), *Actor->GetName());

			if (!bBossEncountered)
			{
				bBossEncountered = true;

				//서버에서 모든 클라이언트에 조우 연출 전파
				Multicast_OnBossEncounter();
			}
		}
		else
		{
			DEBUG_LOG(TEXT("Player lost by Boss: %s"), *Actor->GetName());

			if (bBossEncountered)
			{
				bBossEncountered = false;

				//서버에서 모든 클라이언트에 이탈 연출 전파
				Multicast_OnBossDisengage();
			}
		}
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
