#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItemDataAsset.h"
#include "GameplayEffect.h"
#include "Engine/StreamableManager.h"
#include "UsableItemDataAsset.generated.h"

class UAnimMontage;
class UStaticMesh;

UENUM(BlueprintType)
enum class EUsableItemType : uint8
{
	Tool        UMETA(DisplayName = "도구"),
	Consumable  UMETA(DisplayName = "소모품"),
	Refillable  UMETA(DisplayName = "리필"),
};

UCLASS(BlueprintType)
class ACTIONPRACTICE_API UUsableItemDataAsset : public UBaseItemDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//아이템 타입
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info")
	EUsableItemType ItemType = EUsableItemType::Consumable;

	//최대 보유 수 — Consumable, Refillable에서만 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info", meta = (EditCondition = "ItemType != EUsableItemType::Tool", EditConditionHides, ClampMin = 1))
	int32 MaxStackCount = 1;

	//사용 몽타주 (비동기 로딩용 Soft, Combat 번들로 프리로드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage", meta = (AssetBundles = "Combat"))
	TSoftObjectPtr<UAnimMontage> UseMontage;

	//적용할 GE 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	TSubclassOf<UGameplayEffect> EffectToApply;

	//SetByCaller 수치 (예: 회복량 50)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	float EffectMagnitude = 0.0f;

	//SetByCaller 기간 (0이면 Instant GE 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	float EffectDuration = 0.0f;

	//사용 시 손에 부착할 아이템 메시 (없으면 소품 미표시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	TSoftObjectPtr<UStaticMesh> UseMesh;

	//아이템 메시를 부착할 소켓 이름 (예: hand_l_item)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	FName UseSocketName = NAME_None;

	//소켓 기준 위치/회전/스케일 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	FVector UseMeshOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	FRotator UseMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
	FVector UseMeshScale = FVector::OneVector;

#pragma endregion

#pragma region "Public Functions"

	//AssetManager PrimaryAssetTypesToScan의 "UsableItem" 타입과 매핑 (Combat 번들 ChangeBundleState용)
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("UsableItem"), GetFName());
	}

	//사용 몽타주(Combat 번들) + 사용 메시(StaticMesh, RequestAsyncLoad) 프리로드
	//UseMontage는 ChangeBundleState(Combat)로, UseMesh는 별도 RequestAsyncLoad 핸들로 로드한다 (메커니즘 혼합)
	//ItemManagerComponent의 슬롯 장착/전환 시점에서 fire-and-forget으로 호출된다
	void PreloadAssets();

	virtual void BeginDestroy() override;

	FORCEINLINE bool IsTool() const { return ItemType == EUsableItemType::Tool; }
	FORCEINLINE bool IsConsumable() const { return ItemType == EUsableItemType::Consumable; }
	FORCEINLINE bool IsRefillable() const { return ItemType == EUsableItemType::Refillable; }

	//수량 표시가 필요한 타입인지 여부
	FORCEINLINE bool HasCount() const { return ItemType != EUsableItemType::Tool; }

#pragma endregion

protected:
#pragma region "Protected Variables"
#pragma endregion

#pragma region "Protected Functions"
#pragma endregion

private:
#pragma region "Private Variables"

	//UseMontage용 Combat 번들 로드 핸들 — UPROPERTY 아닌 순수 C++ 멤버
	//ChangeBundleStateForPrimaryAssets 반환 핸들을 보관해 몽타주 참조를 확정적으로 유지한다
	TSharedPtr<FStreamableHandle> BundleHandle;

	//UseMesh(StaticMesh, Visual)용 RequestAsyncLoad 핸들 — 번들과 별도 메커니즘
	//UseMesh는 순수 시각 표현이라 Combat 번들에 넣지 않고 기존 방식 유지 + 데디서버 스킵
	TSharedPtr<FStreamableHandle> PreloadHandle;

	//중복 프리로드 요청 방지 플래그
	bool bPreloadRequested = false;

#pragma endregion

#pragma region "Private Functions"
#pragma endregion
};

USTRUCT(BlueprintType)
struct FUsableItemSlot
{
	GENERATED_BODY()

	//아이템 정의 (DA 레퍼런스)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UUsableItemDataAsset> ItemDA = nullptr;

	//현재 보유 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 CurrentCount = 0;

	//유효한 슬롯인지 확인
	bool IsValid() const { return ItemDA != nullptr; }

	//사용 가능 여부 (수량 남아있거나 도구형)
	bool CanUse() const
	{
		if (!ItemDA) return false;
		if (ItemDA->IsTool()) return true;
		return CurrentCount > 0;
	}

	//수량 차감 (도구형이면 차감하지 않음, 성공 여부 반환)
	bool ConsumeOne()
	{
		if (!ItemDA) return false;
		if (ItemDA->IsTool()) return true;
		if (CurrentCount <= 0) return false;
		CurrentCount--;
		return true;
	}

	//Refillable 타입일 때 최대 수량으로 복원
	void Refill()
	{
		if (!ItemDA) return;
		if (!ItemDA->IsRefillable()) return;
		CurrentCount = ItemDA->MaxStackCount;
	}
};
