#include "GAS/Abilities/Enemy/BossDeathAbility.h"
#include "Characters/BossCharacter.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossDeathAbility, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogBossDeathAbility, Warning, Format, ##__VA_ARGS__)
#else
	#define DEBUG_LOG(Format, ...)
#endif

UBossDeathAbility::UBossDeathAbility()
{
}

void UBossDeathAbility::ExecuteDeathServerLogic()
{
	//공통 사망 처리 (어빌리티 취소, 이동 비활성, AI 정지)
	Super::ExecuteDeathServerLogic();

	//보스 전용 처리
	StopBossBGMAndHideUI();
}

void UBossDeathAbility::StopBossBGMAndHideUI()
{
	ABossCharacter* Boss = GetBossCharacterFromActorInfo();
	if (!Boss) return;

	//BGM 정지 + 보스 HP바 숨김 — Multicast로 모든 클라이언트에 전파
	Boss->Multicast_OnBossDisengage();

	DEBUG_LOG(TEXT("StopBossBGMAndHideUI: Boss BGM stopped, HP bar hidden"));
}
