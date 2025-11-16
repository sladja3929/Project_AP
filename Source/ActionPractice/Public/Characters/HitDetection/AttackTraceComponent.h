#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Items/AttackData.h"
#include "Characters/HitDetection/HitDetectionInterface.h"
#include "GameplayAbilities/Public/GameplayEffectTypes.h"
#include "AttackTraceComponent.generated.h"

class UAbilitySystemComponent;
class UMeshComponent;

USTRUCT()
struct FHitValidationData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY()
	float LastHitTime = 0.0f;

	UPROPERTY()
	int32 HitCount = 0;
};

USTRUCT()
struct FHitSocketGroupConfig
{
	GENERATED_BODY()

	EAttackDamageType AttackMotionType = EAttackDamageType::None;
	FName SocketGroupName = NAME_None;
	int32 SocketCount = 2;
	float TraceRadius = 10.0f;

	UPROPERTY()
	TArray<FName> TraceSocketNames;

	UPROPERTY()
	TArray<FVector> PreviousSocketPositions;

	UPROPERTY()
	TArray<FVector> CurrentSocketPositions;

	// ===== Adaptive Trace Settings (소켓 그룹 별로 적용) =====
	FVector PrevTipSocketLocation = FVector::ZeroVector;
	float CurrentSecondsPerTrace = 1.0f;
	int32 CurrentInterpolationPerTrace = 1;
	float TraceAccumulator = 0.0f;
};

USTRUCT(BlueprintType)
struct FAdaptiveTraceConfig
{
	GENERATED_BODY()

	//트레이스 실행 주기 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Trace")
	float SecondsPerTrace = 0.16;

	//프레임 별 트레이스 보간 횟수 (추가)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Trace")
	int32 InterpolationPerTrace = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Trace")
	float SpeedThreshold = 0;
};

UCLASS(Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONPRACTICE_API UAttackTraceComponent : public UActorComponent, public IHitDetectionInterface
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	FOnHitDetected OnHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Settings")
	float HitCooldownTime = 0.1f;

#pragma endregion

#pragma region "Public Functions"

	UAttackTraceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual AActor* GetOwnerActor() const PURE_VIRTUAL(UAttackTraceComponent::GetOwnerActor, return nullptr;);
	virtual UAbilitySystemComponent* GetOwnerASC() const PURE_VIRTUAL(UAttackTraceComponent::GetOwnerASC, return nullptr;);
	
	// ===== HitDetection Interface =====
	virtual void PrepareHitDetection(const FGameplayTagContainer& AttackTags, const int32 ComboIndex) override;
	virtual void PrepareHitDetection(const FName& AttackName, const int32 ComboIndex) override;

	UFUNCTION()
	virtual void HandleHitDetectionStart(const FGameplayEventData& Payload) override;

	UFUNCTION()
	virtual void HandleHitDetectionEnd(const FGameplayEventData& Payload) override;

	virtual FOnHitDetected& GetOnHitDetected() override { return OnHit; }
	//=====================================

	UFUNCTION(BlueprintCallable, Category = "Attack Trace")
	void ResetHitActors();

#pragma endregion

protected:
#pragma region "Protected Variables"

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshComponent> OwnerMesh = nullptr;

	FFinalAttackData CurrentAttackData;

	// ===== Trace Config Variables =====
	
	//미리 로드된 모든 소켓 정보, Name은 Prefix
	TMap<FName, FHitSocketGroupConfig> PrebuiltSocketGroups;

	//공격 한 번에 사용될 소켓들, Name은 Prefix
	TMap<FName, FHitSocketGroupConfig> UsingHitSocketGroups;
	
	bool bIsTracing = false;
	bool bIsPrepared = false;

	// ===== Adaptive Trace Sweep Variables =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adaptive Trace")
	TArray<FAdaptiveTraceConfig> AdaptiveConfigs = {
		{0.05f, 1, 0.0f},
		{0.04f, 2, 500.0f},
		{0.033f, 2, 1500.0f},
		{0.025f, 3, 2500.0f},
		{0.020f, 3, 3000.0f},
		{0.016f, 3, 5000.0f},
		{0.016f, 4, 6000.0f},
		{0.016f, 5, 10000.0f}
	};

	// ===== Hit Variables =====
	UPROPERTY()
	TMap<AActor*, FHitValidationData> HitValidationMap;
	
	// ===== Event Delegate =====
	FDelegateHandle HitDetectionStartHandle;
	FDelegateHandle HitDetectionEndHandle;

#pragma endregion

#pragma region "Protected Functions"

	// ===== Trace Config Functions =====
	bool UpdateSocketPositions();
	void StartTrace();
	void StopTrace();
	
	//트레이스 설정 로드 (각 클래스마다 데이터 소스가 다름)
	virtual bool LoadTraceConfig(const FGameplayTagContainer& AttackTags, int32 ComboIndex) { return false; }
	virtual bool LoadTraceConfig(const FName& AttackName, int32 ComboIndex) { return false; }

	//Owner의 메시 컴포넌트를 OwnerMesh에 설정 (Weapon은 StaticMesh, Enemy는 SkeletalMesh)
	virtual void SetOwnerMesh() PURE_VIRTUAL(UAttackTraceComponent::SetOwnerMesh, );

	//DataAsset의 HitSocketInfo 배열로부터 PrebuiltSocketConfigs 생성
	void BuildSocketConfigs(const TArray<FHitSocketInfo>& SocketInfoArray);

	// ===== Execute Trace Functions =====
	void PerformTrace(float DeltaTime);
	void PerformSlashTrace(FHitSocketGroupConfig& SocketGroup);
	void PerformPierceTrace(FHitSocketGroupConfig& SocketGroup);
	void PerformStrikeTrace(FHitSocketGroupConfig& SocketGroup);
	
	// ===== Adaptive Trace Sweep Functions =====
	FVector GetTipSocketLocation(const FHitSocketGroupConfig& SocketGroup) const;
	float CalculateSwingSpeed(const FHitSocketGroupConfig& SocketGroup) const;
	void UpdateAdaptiveTraceSettings(FHitSocketGroupConfig& SocketGroup);
	void PerformInterpolationTrace(
			const FVector& StartPrev, const FVector& StartCurr,
			const FVector& EndPrev, const FVector& EndCurr,
			float Radius, int32 InterpolationPerTrace, TArray<FHitResult>& OutHits);

	// ===== Hit Functions =====
	bool ValidateHit(AActor* HitActor, const FHitResult& HitResult, bool bIsMultiHit);
	void ProcessHit(AActor* HitActor, const FHitResult& HitResult);
	virtual void AddIgnoredActors(FCollisionQueryParams& Params) const;

	// ===== Event Functions =====
	void BindEventCallbacks();
	void UnbindEventCallbacks();
	
	// ===== Utility Functions =====
	ECollisionChannel GetTraceChannel() const;
	FCollisionQueryParams GetCollisionQueryParams() const;

#pragma endregion

#pragma region "Debug And Profiling"
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugTrace")
	bool bDrawDebugTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugTrace")
	float DebugTraceDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugTrace")
	FColor DebugTraceColor = FColor::Red;

	int32 DebugSweepTraceCounter = 0;
	
	void DrawDebugSweepTrace(const FVector& StartPrev, const FVector& StartCurr,
							 const FVector& EndPrev, const FVector& EndCurr,
							 float Radius, const FColor& Color);

	void ToggleWeaponDebugTrace();

#pragma endregion
};
