#include "Interaction/PickupItem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemInterface.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"
#include "Items/BaseItemDataAsset.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogPickupItem, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogPickupItem, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

APickupItem::APickupItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	//루트 컴포넌트 — 액터 자체 Transform/Scale 조절용
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	//메시 — 루트와 독립적으로 스케일 조절 가능
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	//상호작용 감지용 구체 콜리전 — 메시 스케일과 무관하게 반경 고정
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(150.f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
}

bool APickupItem::CanInteract(AActor* InInstigator) const
{
	if (!IsValid(InInstigator)) return false;
	if (!ItemDA) return false;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InInstigator);
	if (!ASI) return false;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return false;

	const FGameplayTag StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();
	if (ASC->HasMatchingGameplayTag(StateDeadTag))
	{
		DEBUG_LOG(TEXT("CanInteract: InInstigator has State_Dead tag — blocked"));
		return false;
	}

	return true;
}

void APickupItem::Interact(AActor* InInstigator)
{
	if (!IsValid(InInstigator)) return;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InInstigator);
	if (!ASI) return;

	UBaseAbilitySystemComponent* BaseASC = Cast<UBaseAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!BaseASC) return;

	//Ability.GetItem 태그로 스펙 조회
	const FGameplayTag AbilityGetItemTag = UGameplayTagsSubsystem::GetAbilityGetItemTag();
	if (!AbilityGetItemTag.IsValid()) return;

	TArray<FGameplayAbilitySpec*> GetItemSpecs;
	BaseASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityGetItemTag), GetItemSpecs);
	if (GetItemSpecs.IsEmpty())
	{
		DEBUG_LOG(TEXT("Interact: No GetItem ability spec found for %s"), *GetNameSafe(InInstigator));
		return;
	}

	//OptionalObject=this로 PickupItem 참조 전달 — Bonfire::Interact 패턴과 동일
	FGameplayEventData EventData;
	EventData.OptionalObject = this;
	DEBUG_LOG(TEXT("Interact: TryActivateAbilityWithEventData for %s"), *GetNameSafe(InInstigator));
	BaseASC->TryActivateAbilityWithEventData(GetItemSpecs[0]->Handle, &EventData);
}

FText APickupItem::GetInteractionPrompt() const
{
	//커스텀 텍스트가 설정되어 있으면 우선 사용
	if (!InteractionPromptText.IsEmpty())
	{
		return InteractionPromptText;
	}

	//DA의 DisplayName 사용
	if (ItemDA)
	{
		return ItemDA->DisplayName;
	}

	return FText::GetEmpty();
}

void APickupItem::OnPickedUp()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	DEBUG_LOG(TEXT("OnPickedUp: %s disabled"), *GetName());

	//일정 시간 후 파괴 — 네트워크 환경에서 안전한 지연 파괴
	SetLifeSpan(2.0f);
}
