#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/WidgetComponent.h"
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

#pragma endregion

#pragma region "Public Functions"

	ULockOnComponent();

	FORCEINLINE bool IsLockedOn() const { return bIsLockOn; }
	FORCEINLINE AActor* GetLockOnTarget() const { return LockedOnTarget; }

	void SetLockedOnTarget(AActor* NewTarget);
	void SetRotationMode(bool bOrientToMovement, bool bUseControllerDesired);
	AActor* FindNearestTarget();

	//Controller의 HandleToggleLockOn 로직을 여기로 통합
	void ToggleLockOn();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

protected:
#pragma region "Protected Variables"

#pragma endregion

#pragma region "Protected Functions"

	virtual void BeginPlay() override;

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

#pragma endregion

#pragma region "Private Functions"

	//마커를 타겟에 부착하고 표시
	void ShowLockOnMarker(AActor* Target);

	//마커를 숨기고 분리
	void HideLockOnMarker();

#pragma endregion
};
