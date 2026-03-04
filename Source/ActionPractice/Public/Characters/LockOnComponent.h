#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

#pragma endregion

#pragma region "Private Functions"

#pragma endregion
};
