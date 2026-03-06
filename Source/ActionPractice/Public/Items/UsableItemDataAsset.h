#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItemDataAsset.h"
#include "GameplayEffect.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "UsableItemDataAsset.generated.h"

class UAnimMontage;
class UStaticMesh;

UCLASS(BlueprintType)
class ACTIONPRACTICE_API UUsableItemDataAsset : public UBaseItemDataAsset
{
	GENERATED_BODY()

public:
#pragma region "Public Variables"

	//최대 보유 수 (-1이면 무제한, 도구형)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Info")
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

	//무제한 아이템 여부
	FORCEINLINE bool IsUnlimited() const { return MaxStackCount < 0; }

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

	//사용 가능 여부 (수량 남아있거나 무제한 아이템)
	bool CanUse() const
	{
		if (!ItemDA) return false;
		if (ItemDA->IsUnlimited()) return true;
		return CurrentCount > 0;
	}

	//수량 차감 (무제한이면 차감하지 않음, 성공 여부 반환)
	bool ConsumeOne()
	{
		if (!ItemDA) return false;
		if (ItemDA->IsUnlimited()) return true;
		if (CurrentCount <= 0) return false;
		CurrentCount--;
		return true;
	}
};
