#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItemDataAsset.h"
#include "GameplayEffect.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
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

	//사용 몽타주 (비동기 로딩용 Soft)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Usage")
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

	//몽타주 프리로드
	void PreloadMontage();

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
