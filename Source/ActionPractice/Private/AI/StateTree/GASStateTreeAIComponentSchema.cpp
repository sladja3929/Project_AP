#include "AI/StateTree/GASStateTreeAIComponentSchema.h"

#include "BrainComponent.h"
#include "StateTreeTypes.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTaskBase.h"
#include "AI/EnemyAIController.h"
#include "Characters/BossCharacter.h"
#include "GAS/AbilitySystemComponent/BossAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"


#define ENABLE_DEBUG_LOG 0
#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogBossStateTreeSchema, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogBossStateTreeSchema, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool UGASStateTreeAIComponentSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	//Task, Evaluator, Condition 허용
	return InScriptStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()) ||
		   InScriptStruct->IsChildOf(FStateTreeEvaluatorBase::StaticStruct()) ||
		   InScriptStruct->IsChildOf(FStateTreeConditionBase::StaticStruct());
}

bool UGASStateTreeAIComponentSchema::IsClassAllowed(const UClass* InClass) const
{
	//BossAIController, BossCharacter, Actor 허용
	return InClass->IsChildOf(AEnemyAIController::StaticClass()) ||
		   InClass->IsChildOf(ABossCharacter::StaticClass()) ||
		   InClass->IsChildOf(AActor::StaticClass());
}

bool UGASStateTreeAIComponentSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return true;
}

//언리얼 5.6부터는 GetContextDataDescs로 Context 배열을 가져온 후 Context를 추가
TConstArrayView<FStateTreeExternalDataDesc> UGASStateTreeAIComponentSchema::GetContextDataDescs() const
{
	//Static으로 Context 데이터 배열를 가져와서 메모리 유지 (일반 변수는 에셋 생성 시 에러남)
	static TArray<FStateTreeExternalDataDesc> CachedDescs;

	//매번 새로 생성 (부모 클래스의 Context가 바뀔 수 있으므로)
	CachedDescs.Reset();

	//부모 클래스의 기존 Context 그대로 가져오기 (Actor, AIController)
	TConstArrayView<FStateTreeExternalDataDesc> ParentDescsView = Super::GetContextDataDescs();
	CachedDescs.Append(ParentDescsView.GetData(), ParentDescsView.Num());

	//커스텀 Context 데이터 추가
	
	//ASC
	FStateTreeExternalDataDesc ASCDesc(UAbilitySystemComponent::StaticClass(), EStateTreeExternalDataRequirement::Required);
	ASCDesc.Name = TEXT("AbilitySystemComponent");
	CachedDescs.Add(ASCDesc);
	
	return CachedDescs;
}

void UGASStateTreeAIComponentSchema::SetContextData(FContextDataSetter& ContextDataSetter, bool bLogErrors) const
{
	//기본 Actor, AIController 설정
	Super::SetContextData(ContextDataSetter, bLogErrors);

	//Actor 가져오기
	AAIController* AIOwner = ContextDataSetter.GetComponent()->GetAIOwner();
	AActor* OwnerActor = (AIOwner != nullptr) ? AIOwner->GetPawn() : ContextDataSetter.GetComponent()->GetOwner();
	AActor* FallbackOwner = ContextDataSetter.GetComponent()->GetOwner();

	UAbilitySystemComponent* ASC = nullptr;

	if (OwnerActor)
	{
		//IAbilitySystemInterface를 통해 ASC 가져오기
		if (IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(OwnerActor))
		{
			ASC = ASInterface->GetAbilitySystemComponent();
		}

		//인터페이스가 없으면 컴포넌트로 직접 찾기
		if (!ASC)
		{
			ASC = OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
		}
	}

	//이전 ASC와 비교하여 변경되었을 때만 로그 출력 (로그 스팸 방지)
	static UAbilitySystemComponent* LastASC = nullptr;

	if (ASC != LastASC)
	{
		DEBUG_LOG(TEXT("Schema::SetContextData: ASC Changed! Old=%p, New=%p, Actor=%s"),
			LastASC,
			ASC,
			*GetNameSafe(OwnerActor ? OwnerActor : FallbackOwner));

		LastASC = ASC;
	}

	//ASC를 Context에 설정
	if (!ContextDataSetter.SetContextDataByName(TEXT("AbilitySystemComponent"), FStateTreeDataView(ASC)))
	{
		DEBUG_LOG(TEXT("Schema::SetContextData: FAILED to set ASC in context. ASC=%p"), ASC);
		if (bLogErrors && !ASC)
		{
			DEBUG_LOG(TEXT("Schema::SetContextData: bLogErrors=true and ASC is nullptr"));
		}
	}
}
