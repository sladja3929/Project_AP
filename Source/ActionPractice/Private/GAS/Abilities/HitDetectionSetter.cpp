#include "GAS/Abilities/HitDetectionSetter.h"

#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
	DEFINE_LOG_CATEGORY_STATIC(LogHitDetectionSetter, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(LogHitDetectionSetter, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

bool FHitDetectionSetter::Init(const TScriptInterface<IHitDetectionInterface>& InHitDetection)
{
	//HitDetection을 새 인터페이스로 교체하기 전에 구 컴포넌트 delegate에서 핸들 제거
	//(교체 후 UnBind하면 구 컴포넌트의 delegate 핸들을 제거하지 못하는 버그 방지)
	UnBind();

	HitDetection = InHitDetection;

	if(!HitDetection)
	{
		DEBUG_LOG(TEXT("Init: HitDetection is nullptr"));
		return false;
	}

	DEBUG_LOG(TEXT("Init: HitDetection initialized successfully"));
	return true;
}

bool FHitDetectionSetter::Bind(IHitDetectionUser* User)
{
	if(!HitDetection)
	{
		DEBUG_LOG(TEXT("Bind: HitDetection is nullptr"));
		return false;
	}

	if(!User)
	{
		DEBUG_LOG(TEXT("Bind: User is nullptr"));
		return false;
	}

	UnBind();

	OnHitDelegateHandle = HitDetection->GetOnHitDetected().AddRaw(User, &IHitDetectionUser::OnHitDetected);

	DEBUG_LOG(TEXT("Bind: Successfully bound to IHitDetectionUser"));
	return true;
}

void FHitDetectionSetter::UnBind()
{
	if(OnHitDelegateHandle.IsValid() && HitDetection)
	{
		HitDetection->GetOnHitDetected().Remove(OnHitDelegateHandle);
		OnHitDelegateHandle.Reset();

		DEBUG_LOG(TEXT("UnBind: Delegate unbound"));
	}
}

bool FHitDetectionSetter::PrepareHitDetection(const FGameplayTagContainer& AssetTag, const int32 ComboCounter)
{
	if(!HitDetection)
	{
		DEBUG_LOG(TEXT("PrepareHitDetection: HitDetection is nullptr"));
		return false;
	}

	HitDetection->PrepareHitDetection(AssetTag, ComboCounter);

	DEBUG_LOG(TEXT("PrepareHitDetection: Called with ComboCounter=%d"), ComboCounter);
	return true;
}


bool FHitDetectionSetter::IsValid() const
{
	return HitDetection != nullptr;
}