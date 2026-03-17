#include "Characters/InteractionComponent.h"
#include "Interaction/IInteractable.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/GameplayTagsSubsystem.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogInteractionComponent, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogInteractionComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->GetTimerManager().SetTimer(InteractionTimerHandle, this, &UInteractionComponent::SearchForInteractable, 0.15f, true);
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::TryInteract()
{
	if (!CurrentInteractable.IsValid()) return;

	//Owner의 Recovering 상태 확인 — 모든 IInteractable에 공통 적용
	AActor* Owner = GetOwner();
	if (Owner)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
		{
			if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(UGameplayTagsSubsystem::GetStateRecoveringLocalTag()))
				{
					DEBUG_LOG(TEXT("TryInteract: Blocked by State.Recovering.Local tag"));
					return;
				}
			}
		}
	}

	AActor* Interactable = CurrentInteractable.Get();
	IInteractable* InteractableInterface = Cast<IInteractable>(Interactable);
	if (!InteractableInterface) return;

	if (!InteractableInterface->CanInteract(GetOwner())) return;

	InteractableInterface->Interact(GetOwner());
}

AActor* UInteractionComponent::GetCurrentInteractable() const
{
	return CurrentInteractable.Get();
}

void UInteractionComponent::SearchForInteractable()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	const FVector OwnerLocation = Owner->GetActorLocation();

	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		OwnerLocation,
		FQuat::Identity,
		ECollisionChannel::ECC_WorldDynamic,
		FCollisionShape::MakeSphere(InteractionRadius),
		Params
	);

	AActor* BestCandidate = nullptr;
	float BestDistanceSq = FLT_MAX;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (!OverlapActor) continue;
		if (!OverlapActor->GetClass()->ImplementsInterface(UInteractable::StaticClass())) continue;

		const float DistSq = FVector::DistSquared(OwnerLocation, OverlapActor->GetActorLocation());
		if (DistSq < BestDistanceSq)
		{
			BestDistanceSq = DistSq;
			BestCandidate = OverlapActor;
		}
	}

	if (CurrentInteractable.Get() != BestCandidate)
	{
		CurrentInteractable = BestCandidate;
		OnInteractableChanged.Broadcast(BestCandidate);
		DEBUG_LOG(TEXT("SearchForInteractable: New target = %s"), *GetNameSafe(BestCandidate));
	}
}
