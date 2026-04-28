#include "AI/StateTree/GASStateTreeAIComponentSchema.h"

#include "BrainComponent.h"
#include "StateTreeTypes.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTaskBase.h"
#include "AI/EnemyAIController.h"
#include "Characters/EnemyCharacter.h"
#include "GAS/AbilitySystemComponent/EnemyAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"


#define ENABLE_DEBUG_LOG 0
#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogEnemyStateTreeSchema, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogEnemyStateTreeSchema, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

UGASStateTreeAIComponentSchema::UGASStateTreeAIComponentSchema()
{
	//부모 생성자 이후 ContextDataDescs 상태:
	//  [0] = Name="Actor", Struct=APawn
	//  [1] = Name="AIController", Struct=AAIController

	//[0]: Actor → EnemyCharacter (타입+이름 변경)
	ContextActorClass = AEnemyCharacter::StaticClass();
	ContextDataDescs[0].Struct = ContextActorClass.Get();
	ContextDataDescs[0].Name = FName(TEXT("EnemyCharacter"));

	//[1]: AIController → EnemyAIController (타입+이름 변경)
	AIControllerClass = AEnemyAIController::StaticClass();
	ContextDataDescs[1].Struct = AIControllerClass.Get();
	ContextDataDescs[1].Name = FName(TEXT("EnemyAIController"));

	//[2]: EnemyAbilitySystemComponent 추가
	ContextDataDescs.Emplace(
		FName(TEXT("EnemyAbilitySystemComponent")),
		UEnemyAbilitySystemComponent::StaticClass(),
		FGuid(0xA1B2C3D4, 0xE5F6A7B8, 0xC9D0E1F2, 0x13243546)
	);
}

void UGASStateTreeAIComponentSchema::PostLoad()
{
	Super::PostLoad();

	//Super::PostLoad이 [0].Struct, [1].Struct를 ContextActorClass/AIControllerClass로 복원하지만
	//Name은 직렬화된 옛 이름("Actor", "AIController")이 남아있으므로 강제 보정
	ContextDataDescs[0].Name = FName(TEXT("EnemyCharacter"));
	ContextDataDescs[1].Name = FName(TEXT("EnemyAIController"));

	//기존 에셋은 엔트리 2개만 직렬화되어 있으므로 ASC 엔트리 복원
	if (ContextDataDescs.Num() < 3)
	{
		ContextDataDescs.Emplace(
			FName(TEXT("EnemyAbilitySystemComponent")),
			UEnemyAbilitySystemComponent::StaticClass(),
			FGuid(0xA1B2C3D4, 0xE5F6A7B8, 0xC9D0E1F2, 0x13243546)
		);
	}
	else
	{
		ContextDataDescs[2].Struct = UEnemyAbilitySystemComponent::StaticClass();
		ContextDataDescs[2].Name = FName(TEXT("EnemyAbilitySystemComponent"));
	}
}

bool UGASStateTreeAIComponentSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	if (Super::IsStructAllowed(InScriptStruct))
	{
		return true;
	}

	//게임 모듈 커스텀 Task, Evaluator, Condition 허용
	return InScriptStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()) ||
		   InScriptStruct->IsChildOf(FStateTreeEvaluatorBase::StaticStruct()) ||
		   InScriptStruct->IsChildOf(FStateTreeConditionBase::StaticStruct());
}

bool UGASStateTreeAIComponentSchema::IsClassAllowed(const UClass* InClass) const
{
	//EnemyAIController, EnemyCharacter, Actor 허용
	return InClass->IsChildOf(AEnemyAIController::StaticClass()) ||
		   InClass->IsChildOf(AEnemyCharacter::StaticClass()) ||
		   InClass->IsChildOf(AActor::StaticClass());
}

bool UGASStateTreeAIComponentSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return true;
}

void UGASStateTreeAIComponentSchema::SetContextData(FContextDataSetter& ContextDataSetter, bool bLogErrors) const
{
	//부모는 "Actor"/"AIController" 이름으로 설정하므로 Super 호출하지 않음

	AAIController* AIOwner = ContextDataSetter.GetComponent()->GetAIOwner();
	APawn* OwnerPawn = AIOwner ? AIOwner->GetPawn() : nullptr;

	//EnemyCharacter
	AEnemyCharacter* EnemyChar = Cast<AEnemyCharacter>(OwnerPawn);
	if (!ContextDataSetter.SetContextDataByName(FName(TEXT("EnemyCharacter")), FStateTreeDataView(EnemyChar)))
	{
		if (bLogErrors)
		{
			DEBUG_LOG(TEXT("SetContextData: FAILED to set EnemyCharacter (Pawn=%s)"), *GetNameSafe(OwnerPawn));
		}
	}

	//EnemyAIController
	AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(AIOwner);
	if (!ContextDataSetter.SetContextDataByName(FName(TEXT("EnemyAIController")), FStateTreeDataView(EnemyAI)))
	{
		if (bLogErrors)
		{
			DEBUG_LOG(TEXT("SetContextData: FAILED to set EnemyAIController (AI=%s)"), *GetNameSafe(AIOwner));
		}
	}

	//EnemyAbilitySystemComponent
	UEnemyAbilitySystemComponent* EnemyASC = nullptr;
	if (EnemyChar)
	{
		if (IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(EnemyChar))
		{
			EnemyASC = Cast<UEnemyAbilitySystemComponent>(ASInterface->GetAbilitySystemComponent());
		}
	}

	if (!ContextDataSetter.SetContextDataByName(FName(TEXT("EnemyAbilitySystemComponent")), FStateTreeDataView(EnemyASC)))
	{
		if (bLogErrors)
		{
			DEBUG_LOG(TEXT("SetContextData: FAILED to set EnemyASC (EnemyChar=%s)"), *GetNameSafe(EnemyChar));
		}
	}
}
