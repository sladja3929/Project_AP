#include "Interaction/Bonfire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemInterface.h"
#include "GAS/GameplayTagsSubsystem.h"
#include "GAS/AbilitySystemComponent/BaseAbilitySystemComponent.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBonfire, Log, All);
	#define DEBUG_LOG(Format, ...) UE_LOG(LogBonfire, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

ABonfire::ABonfire()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	//루트 컴포넌트 설정
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	//상호작용 감지용 구체 콜리전
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(150.f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	//리스폰 포인트
	RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(RootComponent);
}

bool ABonfire::CanInteract(AActor* InInstigator) const
{
	if (!IsValid(InInstigator)) return false;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InInstigator);
	if (!ASI) return false;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return false;

	const FGameplayTag StateDeadTag = UGameplayTagsSubsystem::GetStateDeadTag();
	const FGameplayTag StateRestingTag = UGameplayTagsSubsystem::GetStateRestingTag();

	if (ASC->HasMatchingGameplayTag(StateDeadTag))
	{
		DEBUG_LOG(TEXT("CanInteract: InInstigator has State_Dead tag — blocked"));
		return false;
	}

	if (ASC->HasMatchingGameplayTag(StateRestingTag))
	{
		DEBUG_LOG(TEXT("CanInteract: InInstigator has State_Resting tag — blocked"));
		return false;
	}

	return true;
}

void ABonfire::Interact(AActor* InInstigator)
{
	if (!IsValid(InInstigator)) return;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InInstigator);
	if (!ASI) return;

	UBaseAbilitySystemComponent* BaseASC = Cast<UBaseAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	if (!BaseASC) return;

	//AbilityRest 태그로 스펙 조회
	const FGameplayTag AbilityRestTag = UGameplayTagsSubsystem::GetAbilityRestTag();
	if (!AbilityRestTag.IsValid()) return;

	TArray<FGameplayAbilitySpec*> RestSpecs;
	BaseASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(AbilityRestTag), RestSpecs);
	if (RestSpecs.IsEmpty())
	{
		DEBUG_LOG(TEXT("Interact: No Rest ability spec found for %s"), *GetNameSafe(InInstigator));
		return;
	}

	//OptionalObject=this로 Bonfire 참조 전달
	FGameplayEventData EventData;
	EventData.OptionalObject = this;
	DEBUG_LOG(TEXT("Interact: TryActivateAbilityWithEventData for %s"), *GetNameSafe(InInstigator));
	BaseASC->TryActivateAbilityWithEventData(RestSpecs[0]->Handle, &EventData);
}

FText ABonfire::GetInteractionPrompt() const
{
	return InteractionPromptText;
}
