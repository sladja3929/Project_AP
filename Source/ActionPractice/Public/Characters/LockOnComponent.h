#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Characters/LockOn/LockOnTargetSelector.h"
#include "LockOnComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//락온 최대 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float LockOnMaxDistance = 2000.0f;

	//락온 대상 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	FName TargetActorTag = FName("Enemy");

	//락온 마커 위젯 클래스 (BP에서 설정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Marker")
	TSubclassOf<UUserWidget> LockOnMarkerWidgetClass;

	//마커를 부착할 스켈레탈 메시 소켓 이름 (비어있으면 루트 컴포넌트에 부착)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Marker")
	FName LockOnMarkerSocketName = FName("LockOnSocket");

	//마커 위치 오프셋 (소켓 또는 루트 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Marker")
	FVector LockOnMarkerOffset = FVector(0.f, 0.f, 0.f);

	//마커 위젯 드로우 크기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Marker")
	FVector2D LockOnMarkerDrawSize = FVector2D(64.f, 64.f);

	//탈락 조건: 카메라 전방과 타겟 사이 최대 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Scoring")
	float MaxAngleDegrees = 60.0f;

	//선호 점수: 각도 가중치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Scoring")
	float AngleWeight = 0.7f;

	//선호 점수: 거리 가중치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Scoring")
	float DistanceWeight = 0.3f;

	//전환 발동에 필요한 누적 시점 입력 크기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch")
	float SwitchInputThreshold = 70.0f;

	//전환 직후 입력 누적을 무시하는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch")
	float SwitchCooldown = 0.25f;

	//전환 비용의 화면 거리 페널티 가중치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch")
	float SwitchDistancePenaltyWeight = 0.5f;

	//이 시간 이상 시점 입력이 없으면 누적값을 리셋 (미세 입력 누적으로 인한 오전환 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch")
	float SwitchInputResetTime = 0.2f;

	//상하 전환 입력 감쇠 계수
	//IA_Look의 Y 부호는 프로젝트 입력 모디파이어(Negate)에 따라 달라지므로 기본값 0으로 두고 좌우 전환만 사용한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch")
	float SwitchVerticalInputScale = 0.0f;

	//가려진 후보를 제외하며 재탐색하는 최대 반복 횟수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch")
	int32 SwitchVisibilityMaxIterations = 4;

	//해제 거리 배수 (시작 거리보다 크게 둬 떨림 막음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Maintenance")
	float ReleaseDistanceMultiplier = 1.2f;

	//가림이 이 시간 이상 지속되면 해제
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Maintenance")
	float LoseSightGraceTime = 1.0f;

	//유지 검사 주기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Maintenance")
	float LockOnMaintenanceInterval = 0.1f;

	//타겟을 잃었을 때 자동 재지정 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Maintenance")
	bool bAutoRetargetOnTargetLost = true;

	//서버 거리 검증 허용 오차
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Validation")
	float ServerValidationTolerance = 200.0f;

#pragma endregion

#pragma region "Public Functions"

	ULockOnComponent();

	FORCEINLINE bool IsLockedOn() const { return bIsLockOn; }
	FORCEINLINE AActor* GetLockOnTarget() const { return LockedOnTarget; }

	void SetLockedOnTarget(AActor* NewTarget);
	void SetRotationMode(bool bOrientToMovement, bool bUseControllerDesired);

	//타겟 찾기
	AActor* FindBestTarget(const AActor* ExcludedActor = nullptr);

	//Controller의 HandleToggleLockOn 로직을 여기로 통합
	void ToggleLockOn();

	//좌우 타겟 전환
	void AccumulateSwitchInput(const FVector2D& LookAxis);

	//타겟이 사망/소멸해 잃었을 때 재지정 또는 해제
	void HandleTargetLost(AActor* LostTarget);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//락온 상태를 서버에 동기화
	UFUNCTION(Server, Reliable)
	void ServerSetLockOnState(bool bNewLockOn, AActor* NewTarget);

	//CMC 회전 모드를 서버에 동기화
	UFUNCTION(Server, Reliable)
	void ServerSetRotationMode(bool bOrientToMovement, bool bUseControllerDesired);

#pragma endregion

private:
#pragma region "Private Variables"

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LockOn", Replicated, meta = (AllowPrivateAccess = "true"))
	bool bIsLockOn = false;

	UPROPERTY(BlueprintReadOnly, Category = "LockOn", Replicated, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> LockedOnTarget = nullptr;

	//현재 회전 모드 캐싱 (불필요한 RPC 방지)
	bool bCachedOrientToMovement = true;
	bool bCachedUseControllerDesired = false;

	//로컬에서만 관리하는 락온 마커 위젯 컴포넌트
	TObjectPtr<UWidgetComponent> LockOnMarkerWidget = nullptr;

	//누적된 시점 입력 (로컬 전용)
	FVector2D AccumulatedSwitchInput = FVector2D::ZeroVector;

	//마지막 전환 시각
	float LastSwitchTime = -FLT_MAX;

	//마지막 시점 입력 시각
	float LastSwitchInputTime = -FLT_MAX;

	//락 유지 검사 타이머 (로컬 전용)
	FTimerHandle LockOnMaintenanceTimerHandle;

	//가림 누적 시간
	float LostSightAccumulatedTime = 0.0f;

#pragma endregion

#pragma region "Private Functions"

	//마커를 타겟에 부착하고 표시
	void ShowLockOnMarker(AActor* Target);

	//마커를 숨기고 분리
	void HideLockOnMarker();

	//로컬 PlayerCameraManager 기준 카메라 스냅샷 생성. PC/카메라 매니저가 없으면 false
	bool BuildLockOnView(FLockOnView& OutView) const;

	//UPROPERTY 튜닝 값으로 점수 파라미터 구성
	FLockOnScoringParams MakeScoringParams() const;

	//반경 내 후보를 물리 씬 공간 질의로 수집
	void GatherCandidates(TArray<FLockOnCandidate>& OutCandidates, const AActor* ExcludedActor = nullptr) const;

	//조준 기준점. 마커 소켓이 있으면 소켓 위치, 없으면 액터 위치
	FVector GetAimPoint(const AActor* Target) const;

	//지형/구조물에 가려졌는지 검사
	bool IsTargetVisible(const FLockOnView& View, const FVector& AimPoint) const;

	//타겟 탐색/전환/유지 로직이 실행 가능한 로컬 컨트롤 상태인지
	bool IsLocallyControlledOwner() const;

	//주어진 화면 방향으로 인접 타겟 전환을 시도
	bool TrySwitchTarget(const FVector2D& InputDir);

	//락 유지 검사 타이머 시작/정지
	void StartLockOnMaintenance();
	void StopLockOnMaintenance();

	//저빈도 락 유지 검사 (현재 타겟 1명만 보므로 O(1))
	void TickLockOnMaintenance();

	//서버에서 클라이언트 락온 요청을 최소 검증
	bool ValidateLockOnRequest(const AActor* NewTarget) const;

#pragma endregion
};
