#include "Effects/SweepRevealComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Actor.h"

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogSweepRevealComponent, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogSweepRevealComponent, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

const FName USweepRevealComponent::ParamRevealAlpha = TEXT("RevealAlpha");
const FName USweepRevealComponent::ParamRevealMinZ = TEXT("RevealMinZ");
const FName USweepRevealComponent::ParamRevealMaxZ = TEXT("RevealMaxZ");
const FName USweepRevealComponent::ParamEdgeRadius = TEXT("User.EdgeRadius");

//스윕 종료 후 경계 높이 (월드 Z, 맵 범위 밖)
static constexpr float FinalCutoffDistance = 1.0e7f;

USweepRevealComponent::USweepRevealComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	//메시 이동·애니메이션 반영 후 바운드를 읽도록
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USweepRevealComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SweepElapsed += DeltaTime;

	if (bEdgeEffectPending && SweepElapsed >= 0.0f)
	{
		bEdgeEffectPending = false;
		StartEdgeEffect();
	}

	const float Progress = SweepDuration > 0.0f ? FMath::Clamp(SweepElapsed / SweepDuration, 0.0f, 1.0f) : 1.0f;

	ApplyAlpha(bSweepReveal ? Progress : 1.0f - Progress);

	if (Progress >= 1.0f)
	{
		FinishSweep();
	}
}

#pragma region "Sweep"

USweepRevealComponent* USweepRevealComponent::PlaySweepOnMesh(UMeshComponent* Mesh, bool bReveal, float Duration, UNiagaraSystem* InEdgeEffect, bool bInDestroyOwnerWhenHidden, float StartDelay)
{
	AActor* Owner = Mesh ? Mesh->GetOwner() : nullptr;

	if (!Owner)
	{
		DEBUG_LOG(TEXT("PlaySweepOnMesh: Mesh or owner is null"));
		return nullptr;
	}

	//화면이 없는 데디 서버에서는 연출 생략 (호출 측 가드의 이중 안전장치)
	if (Owner->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	USweepRevealComponent* SweepComponent = Owner->FindComponentByClass<USweepRevealComponent>();

	if (!SweepComponent)
	{
		SweepComponent = NewObject<USweepRevealComponent>(Owner);
		SweepComponent->RegisterComponent();
	}

	SweepComponent->EdgeEffect = InEdgeEffect;
	SweepComponent->bDestroyOwnerWhenHidden = bInDestroyOwnerWhenHidden;
	SweepComponent->PlaySweep(Mesh, bReveal, Duration, StartDelay);

	return SweepComponent;
}

void USweepRevealComponent::PlaySweep(UMeshComponent* Mesh, bool bReveal, float Duration, float StartDelay)
{
	if (!Mesh)
	{
		return;
	}

	StopEdgeEffect();

	//머티리얼 슬롯마다 MID 생성 (이미 MID면 그대로 반환)
	if (TargetMesh.Get() != Mesh)
	{
		TargetMaterials.Reset();

		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
			{
				TargetMaterials.Add(MID);
			}
		}

		if (TargetMaterials.Num() == 0)
		{
			DEBUG_LOG(TEXT("PlaySweep: %s has no dynamic material (needs a material using MF_SweepReveal)"), *GetNameSafe(Mesh));
		}
	}

	TargetMesh = Mesh;
	bSweepReveal = bReveal;
	SweepDuration = FMath::Max(Duration, 0.0f);
	SweepElapsed = -FMath::Max(StartDelay, 0.0f);

	//시작 프레임(지연 중 포함)에 완성 상태가 보이지 않도록 즉시 시작값 적용
	ApplyAlpha(bReveal ? 0.0f : 1.0f);

	bEdgeEffectPending = SweepElapsed < 0.0f;

	if (!bEdgeEffectPending)
	{
		StartEdgeEffect();
	}

	SetComponentTickEnabled(true);
}

void USweepRevealComponent::StartEdgeEffect()
{
	const UMeshComponent* Mesh = TargetMesh.Get();

	if (!EdgeEffect || !Mesh)
	{
		return;
	}

	//위치·반경을 먼저 넣고 활성화 (첫 프레임에 기본 반경으로 보이지 않도록)
	EdgeComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EdgeEffect, Mesh->Bounds.Origin, FRotator::ZeroRotator, FVector::OneVector, true, false, ENCPoolMethod::None);

	const float Progress = SweepDuration > 0.0f ? FMath::Clamp(SweepElapsed / SweepDuration, 0.0f, 1.0f) : 1.0f;
	ApplyAlpha(bSweepReveal ? Progress : 1.0f - Progress);

	if (UNiagaraComponent* Edge = EdgeComponent.Get())
	{
		Edge->Activate(true);
	}
}

void USweepRevealComponent::ApplyAlpha(float Alpha)
{
	const UMeshComponent* Mesh = TargetMesh.Get();

	if (!Mesh)
	{
		return;
	}

	const FBoxSphereBounds& Bounds = Mesh->Bounds;
	const float MinZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	const float MaxZ = Bounds.Origin.Z + Bounds.BoxExtent.Z;

	for (UMaterialInstanceDynamic* MID : TargetMaterials)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(ParamRevealAlpha, Alpha);
			MID->SetScalarParameterValue(ParamRevealMinZ, MinZ);
			MID->SetScalarParameterValue(ParamRevealMaxZ, MaxZ);
		}
	}

	if (UNiagaraComponent* Edge = EdgeComponent.Get())
	{
		const FVector EdgeLocation(Bounds.Origin.X, Bounds.Origin.Y, FMath::Lerp(MinZ, MaxZ, Alpha));
		Edge->SetWorldLocation(EdgeLocation);
		Edge->SetVariableFloat(ParamEdgeRadius, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * EdgeRadiusScale);
	}
}

void USweepRevealComponent::FinishSweep()
{
	SetComponentTickEnabled(false);
	StopEdgeEffect();

	//끝난 뒤 메시가 움직여도(던져진 바위 등) 마지막 바운드에 잘리지 않도록 경계를 멀리 고정
	const float FinalCutoffZ = bSweepReveal ? FinalCutoffDistance : -FinalCutoffDistance;

	for (UMaterialInstanceDynamic* MID : TargetMaterials)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(ParamRevealAlpha, bSweepReveal ? 1.0f : 0.0f);
			MID->SetScalarParameterValue(ParamRevealMinZ, FinalCutoffZ);
			MID->SetScalarParameterValue(ParamRevealMaxZ, FinalCutoffZ);
		}
	}

	DEBUG_LOG(TEXT("FinishSweep: %s Revealed=%d"), *GetNameSafe(GetOwner()), bSweepReveal);
	OnSweepFinished.Broadcast(bSweepReveal);

	if (!bSweepReveal && bDestroyOwnerWhenHidden)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->Destroy();
		}
	}
}

void USweepRevealComponent::StopEdgeEffect()
{
	//스폰을 멈추고 남은 입자는 수명대로 사라지게 둠 (자동 제거)
	if (UNiagaraComponent* Edge = EdgeComponent.Get())
	{
		Edge->Deactivate();
	}

	EdgeComponent.Reset();
}

#pragma endregion

void USweepRevealComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEdgeEffect();

	Super::EndPlay(EndPlayReason);
}
