# MoveToDynamicForce 분석 보고서

> 분석 대상 엔진: UE 5.7 설치본 (`C:/Program Files/Epic Games/UE_5.7/`)
>
> 핵심 파일:
> - `Engine/Source/Runtime/Engine/Classes/GameFramework/RootMotionSource.h`
> - `Engine/Source/Runtime/Engine/Private/GameFramework/RootMotionSource.cpp`
> - `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToActorForce.cpp`
> - `Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`
>
> **사전 발견 사항(중요):** UE 5.7 트리에는 `AbilityTask_ApplyRootMotionMoveToDynamicForce.h/.cpp` 파일이 **존재하지 않는다**. `FRootMotionSource_MoveToDynamicForce` struct만 RootMotionSource에 정의되어 있고, 이를 사용하는 GAS task wrapper는 사실상 `UAbilityTask_ApplyRootMotionMoveToActorForce` 한 종류다 (`AbilityTask_ApplyRootMotionMoveToActorForce.cpp:178` 에서 `MakeShared<FRootMotionSource_MoveToDynamicForce>()`로 생성). 따라서 본 보고서의 GAS wrapper 분석은 MoveToActorForce를 기준으로 한다.

---

## 1. struct 정의 및 상속 구조

`RootMotionSource.h:608-664` 정의.

| 항목 | 값 |
| --- | --- |
| 부모 클래스 | `FRootMotionSource` (`MoveToForce`가 아니라 **베이스 직접 상속**) |
| `StartLocation` | `FVector`, UPROPERTY |
| `InitialTargetLocation` | `FVector`, UPROPERTY (생성 시점 target 캐시) |
| `TargetLocation` | `FVector`, UPROPERTY, "Dynamically-changing location of target, which may be altered while this movement is ongoing" 주석 (h:623) |
| `bRestrictSpeedToExpected` | `bool` |
| `PathOffsetCurve` | `TObjectPtr<UCurveVector>` |
| `TimeMappingCurve` | `TObjectPtr<UCurveFloat>` (★ MoveToForce에는 없음) |

**override 함수 목록 (h:636-664):**
- `SetTargetLocation(FVector)` — `cpp:855-858`, **public API**로 외부에서 매 tick target을 갱신할 수 있음
- `GetPathOffsetInWorldSpace(float)` — HeightCurve 적용
- `Clone`, `Matches`, `MatchesAndHasSameState`, `UpdateStateFrom`, `SetTime`, `PrepareRootMotion`, `NetSerialize`, `GetScriptStruct`, `ToSimpleString`, `AddReferencedObjects`

**MoveToForce 대비 차이점:**
- `InitialTargetLocation` 추가 (원래 의도된 목적지를 보존)
- `TimeMappingCurve` 추가 (시간축 비선형 매핑)
- `SetTargetLocation` public 메서드 — 외부에서 target 갱신
- `UpdateStateFrom`이 의미 있게 override됨 (MoveToForce는 빈 스텁)

---

## 2. PrepareRootMotion 동작 분석

`RootMotionSource.cpp:933-1014`

### 2-1. target 갱신 메커니즘 — Q2-1

**`PrepareRootMotion` 본체 안에서는 TargetLocation을 갱신하지 않는다.** 멤버에 actor pointer도 없고, 내부 callback도 없다. 외부에서 명시적으로 `SetTargetLocation()`을 호출해야 갱신된다 (`cpp:855-858`).

```cpp
// RootMotionSource.cpp:855-858
void FRootMotionSource_MoveToDynamicForce::SetTargetLocation(FVector NewTargetLocation)
{
    TargetLocation = NewTargetLocation;
}
```

→ struct 자체는 **수동 push 모델**이다. 누가 갱신하느냐는 GAS task wrapper(MoveToActorForce)가 책임진다 — Phase 4에서 다룸.

### 2-2. lerp 시작점 처리 (★ 핵심) — Q2-2

```cpp
// RootMotionSource.cpp:945-957
float MoveFraction = (GetTime() + SimulationTime) / Duration;

if (TimeMappingCurve)
{
    MoveFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, MoveFraction);
}

FVector CurrentTargetLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
CurrentTargetLocation += GetPathOffsetInWorldSpace(MoveFraction);

const FVector CurrentLocation = Character.GetActorLocation();

FVector Force = (CurrentTargetLocation - CurrentLocation) / MovementTickTime;
```

**결론: 가능성 X (캐싱된 StartLocation 멤버 사용).** Lerp 시작점은 처음 등록 시 캐싱된 `StartLocation` 멤버다. 캐릭터 현재 위치는 lerp의 시작점이 **아니다**.

다만 캐릭터 현재 위치는 두 번째 단계에서 사용된다 — 952번 줄에서 `CurrentTargetLocation`(이번 tick에 "있어야 할 곳")을 구한 뒤, 957번 줄에서 `(CurrentTargetLocation - CurrentLocation) / MovementTickTime`로 Force를 산출. 이 force가 다음 tick까지 캐릭터를 `CurrentTargetLocation`으로 끌어당긴다. **즉 lerp 곡선 자체는 고정 StartLocation 기준이고, 매 tick 캐릭터를 그 곡선상의 "지금 시점 위치"로 velocity 보정한다.** MoveToForce(`cpp:735-806`)와 산출 공식이 동일하다.

**핵심 함의:** TargetLocation이 mid-flight에 크게 바뀌면, 952번 줄의 lerp 결과 `CurrentTargetLocation`도 즉시 바뀐다. 이전 tick의 character 위치와 새 `CurrentTargetLocation` 사이의 force 한 번이 한 tick 안에 캐릭터를 새 위치로 강제로 끌어다 놓는다 → **여전히 시각적 도약 가능성이 존재한다.** MoveToDynamicForce 자체는 자동 보정 메커니즘을 가지고 있지 않다.

### 2-3. PathOffsetCurve / Alpha 계산 — Q2-3

```cpp
// RootMotionSource.cpp:919-931
FVector FRootMotionSource_MoveToDynamicForce::GetPathOffsetInWorldSpace(const float MoveFraction) const
{
    if (PathOffsetCurve)
    {
        const FVector PathOffsetInFacingSpace = EvaluateVectorCurveAtFraction(*PathOffsetCurve, MoveFraction);
        FRotator FacingRotation((TargetLocation-StartLocation).Rotation());
        FacingRotation.Pitch = 0.f;
        return FacingRotation.RotateVector(PathOffsetInFacingSpace);
    }
    return FVector::ZeroVector;
}
```

- HeightCurve 적용 방식은 MoveToForce(`cpp:721-733`)와 **동일**.
- Alpha(MoveFraction)는 누적 시간 기반: `(GetTime() + SimulationTime) / Duration` (`cpp:945`). target이 변해도 Alpha는 reset되지 **않는다**.
- TimeMappingCurve가 지정돼 있으면 `EvaluateFloatCurveAtFraction(*TimeMappingCurve, MoveFraction)`으로 다시 매핑(`cpp:947-950`). 이는 시간축 비선형 가속/감속용. MoveToForce에는 없는 기능.
- `FacingRotation`은 `(TargetLocation-StartLocation).Rotation()`이므로, **TargetLocation이 변하면 facing이 회전한다** → HeightCurve가 만들어 내는 활 모양 궤적의 방향도 함께 회전. 이것이 도약을 발생시키지는 않지만, target이 옆으로 크게 움직이면 곡선의 측면 offset이 흔들릴 수 있다.

### 2-4. 시간 진행 처리 — Q2-4

```cpp
// RootMotionSource.cpp:912-917
void FRootMotionSource_MoveToDynamicForce::SetTime(float NewTime)
{
    FRootMotionSource::SetTime(NewTime);
    // TODO-RootMotionSource: Check if reached destination?
}
```

- `SetTime` override는 베이스를 그대로 호출만 하고 추가 처리 없음 — MoveToForce(`cpp:714-719`)와 동일.
- `PrepareRootMotion` 마지막 줄(`cpp:1013`) `SetTime(GetTime() + SimulationTime)`로 누적. MoveToForce와 동일한 패턴.
- 즉 Source를 새로 만들지 않는 한 **CurrentTime은 끊김 없이 누적되고 진행도가 보존된다**.

---

## 3. 네트워크 동기화 분석

### 3-1. Matches — Q3-1

```cpp
// RootMotionSource.cpp:866-879
bool FRootMotionSource_MoveToDynamicForce::Matches(const FRootMotionSource* Other) const
{
    if (!FRootMotionSource::Matches(Other))
    {
        return false;
    }
    const FRootMotionSource_MoveToDynamicForce* OtherCast = static_cast<const FRootMotionSource_MoveToDynamicForce*>(Other);
    return bRestrictSpeedToExpected == OtherCast->bRestrictSpeedToExpected &&
        PathOffsetCurve == OtherCast->PathOffsetCurve &&
        TimeMappingCurve == OtherCast->TimeMappingCurve;
}
```

비교 항목: `bRestrictSpeedToExpected`, `PathOffsetCurve`, `TimeMappingCurve`만. 베이스 `Matches`(`cpp:231-240`)는 ScriptStruct, Priority, AccumulateMode, bInLocalSpace, InstanceName, Duration을 비교.

**★ TargetLocation, StartLocation, InitialTargetLocation은 Matches 비교 대상이 아니다.**

대조: MoveToForce(`cpp:678-691`)는 `FVector::PointsAreNear(TargetLocation, OtherCast->TargetLocation, 0.1f)`로 **TargetLocation 일치까지 요구**. 본 프로젝트가 MoveToForce에서 ensure trip을 본 정확한 메커니즘이 이 줄이다 — 서버에서 target이 1cm만 이동해도 Matches 실패 → `ConvertRootMotionServerIDsToLocalIDs`(CharacterMovementComponent.cpp:11938-12027)에서 ServerID/LocalID mapping은 매칭됐는데 Matches는 false → `ensureMsgf(false, ...)` 트립(`cpp:12017-12019`).

MoveToDynamicForce는 설계 의도부터 이 충돌을 회피한다. target이 변해도 Matches는 통과하고, 차이는 `MatchesAndHasSameState`/`UpdateStateFrom`을 통해 머지된다.

### 3-2. UpdateStateFrom — Q3-2

```cpp
// RootMotionSource.cpp:896-910
bool FRootMotionSource_MoveToDynamicForce::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
    if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
    {
        return false;
    }
    const FRootMotionSource_MoveToDynamicForce* OtherCast = static_cast<const FRootMotionSource_MoveToDynamicForce*>(SourceToTakeStateFrom);
    StartLocation = OtherCast->StartLocation;
    TargetLocation = OtherCast->TargetLocation;
    return true;
}
```

`MatchesAndHasSameState`는 `cpp:881-894`에서 StartLocation/TargetLocation까지 비교 (`(StartLocation.Equals(...) && TargetLocation.Equals(...))`).
- 즉 server-replicated source의 StartLocation/TargetLocation이 다르면 `MatchesAndHasSameState` → false → `UpdateStateFrom` 호출됨 → server 값이 simulated proxy local source에 머지됨.
- 베이스 `UpdateStateFrom`(`cpp:254-282`)이 `Status`, `CurrentTime`도 함께 머지하므로 **시간 진행도까지 동기화**됨.

**시뮬프록시 처리 흐름:** `CharacterMovementComponent.cpp:1943-1949`의 `SimulatedTick` 경로에서:
1. `ConvertRootMotionServerIDsToLocalIDs(...)` — Matches 통과(인 경우 mapping 유지) — MoveToDynamicForce는 위치 변화에도 통과
2. `CurrentRootMotion.UpdateStateFrom(RootMotionRepMove.RootMotion.AuthoritativeRootMotion, true)` — 매 update 새 server target이 local source에 머지됨

이 점이 MoveToForce와의 본질적 차이다. MoveToForce는 (1)에서 ensure trip → 매핑이 깨짐 → server source가 local source로 머지되지 못함.

### 3-3. NetSerialize — Q3-3

```cpp
// RootMotionSource.cpp:1016-1032
bool FRootMotionSource_MoveToDynamicForce::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess)) return false;
    Ar << StartLocation;
    Ar << InitialTargetLocation;
    Ar << TargetLocation;
    Ar << bRestrictSpeedToExpected;
    Ar << PathOffsetCurve;
    Ar << TimeMappingCurve;
    bOutSuccess = true;
    return true;
}
```

- TargetLocation이 매번 직렬화 대상에 포함됨. CMC가 `RootMotionRepMove`를 보낼 때마다 **현재 TargetLocation이 함께 복제된다.**
- actor reference나 callback delegate 같은 포인터/UObject 멤버는 없음 (`PathOffsetCurve`, `TimeMappingCurve`만 UObject 포인터인데 둘 다 일반 archive `<<` 처리 — 불변 자산이라 NetGUID로 처리됨).
- Quantization은 미적용 (TODO 주석).

**즉 source 등록 시점뿐 아니라 매 RootMotion replication 사이클마다 최신 TargetLocation이 simulated proxy까지 흐른다.**

---

## 4. GAS Task Wrapper 동작

### 4-1. MoveToDynamicForce — UE 5.7에 별도 GAS task wrapper가 없음

검색 결과 (`AbilityTask_ApplyRootMotion*` glob, "MoveToDynamicForce" grep): UE 5.7 트리에 `AbilityTask_ApplyRootMotionMoveToDynamicForce.h/.cpp` 파일은 존재하지 않는다. struct는 GAS task `MoveToActorForce` 안에서만 사용된다(`MoveToActorForce.cpp:178`). UAbilitySystem 외부 코드에서 직접 source를 만들어 `ApplyRootMotionSource` 하는 패턴은 가능하지만, "DynamicForce 전용 GAS task"라는 사용자 추가 wrapper는 엔진에 없다.

### 4-2. MoveToActorForce — `AbilityTask_ApplyRootMotionMoveToActorForce.cpp`

**source 생성 및 등록 흐름 (`SharedInitAndApply`, cpp:153-202):**
```cpp
// cpp:178-193
TSharedPtr<FRootMotionSource_MoveToDynamicForce> MoveToActorForce = MakeShared<FRootMotionSource_MoveToDynamicForce>();
MoveToActorForce->InstanceName = ForceName;
MoveToActorForce->AccumulateMode = ERootMotionAccumulateMode::Override;
MoveToActorForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
MoveToActorForce->Priority = 900;
MoveToActorForce->InitialTargetLocation = TargetLocation;
MoveToActorForce->TargetLocation = TargetLocation;
MoveToActorForce->StartLocation = StartLocation;
MoveToActorForce->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
MoveToActorForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
MoveToActorForce->PathOffsetCurve = PathOffsetCurve;
MoveToActorForce->TimeMappingCurve = TimeMappingCurve;
...
RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToActorForce);
```

`StartLocation`은 task 생성 시 1회 결정 (`cpp:86`: `MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation()`) — 그 후 갱신되지 **않는다**.

**target 갱신 콜백 — `TickTask` (cpp:305-363):**

매 tick:
1. `UpdateTargetLocation(DeltaTime)`(`cpp:235-284`) 호출 — TargetActor의 현재 위치를 가져와 `CalculateTargetOffset`으로 계산 후, **TargetLerpSpeedHorizontalCurve / TargetLerpSpeedVerticalCurve로 변화량을 rate-limit**(`cpp:248-276`):
   ```cpp
   const float MaxHorizontalChange = FMath::Max(0.f, TargetLerpSpeedHorizontal * DeltaTime);
   ...
   if (FMath::Abs(ToExactLocation.SizeSquared2D()) > MaxHorizontalChange*MaxHorizontalChange) { ... clamp ... }
   TargetLocation += TargetLocationDelta;
   ```
2. `SetRootMotionTargetLocation(TargetLocation)`(`cpp:286-303`) 호출 — `MovementComponent->GetRootMotionSourceByID(RootMotionSourceID)`로 source 조회 후 `MoveToActorForce->SetTargetLocation(TargetLocation)`.
3. `bReachedDestination` 체크(`cpp:341`) → 도달했거나 timeout이면 종료.

**중요한 시뮬프록시 처리:** `OnRep_TargetLocation`(`cpp:137-151`)이 simulated proxy에서 호출됨. `bIsSimulating`인 경우 동일하게 `SetRootMotionTargetLocation(TargetLocation)`로 source의 TargetLocation을 갱신. `cpp:370`에서 `DOREPLIFETIME_CONDITION(..., TargetLocation, COND_SimulatedOnly)` — server/owning client는 자기가 직접 계산하고, simulated proxy만 replicated TargetLocation을 받는다. (원격 simulated proxy도 ServerActor의 위치를 직접 알 수 없을 수 있고, lerp curve 등 로컬 환경 차이 때문)

### 4-3. lifecycle — Q4-3

- 종료 조건(`cpp:343`): `bTimedOut || (bReachedDestination && !bDisableDestinationReachedInterrupt)`
- 종료 시 `OnDestroy`(`cpp:392-410`):
  - `MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID)` — source 제거
  - `bSetNewMovementMode`였다면 movement mode 복원
- target actor가 사라지면 `UpdateTargetLocation`이 false 반환 → 마지막 TargetLocation 유지하며 계속 진행 (`cpp:328` 주석).

---

## 5. EnemyLungeAbility 적용 평가

> 본 프로젝트의 lunge가 풀어야 했던 문제 두 가지(commit 메시지 기준):
> (a) **dedi 환경 시뮬프록시 서버-로컬 불일치 ensure trip** → "Source 자체 재생성, StartLocation 역산"으로 우회
> (b) **target 변화 시 급격한 궤적 변화** → "수동 주기 추가: 점진적 변경"

### 5-1. 도약 회피 — **Yes**, 단 조건부

**dedi 환경 ensure trip은 발생하지 않는다.** 이유는 명확하다 — `FRootMotionSource_MoveToDynamicForce::Matches`(`cpp:866-879`)가 TargetLocation/StartLocation을 비교 대상에서 제외하기 때문. server/local source의 위치가 달라도 Matches는 통과하고, `UpdateStateFrom`(`cpp:896-910`)이 server 위치를 매 simulated tick 머지한다(`CharacterMovementComponent.cpp:1949`).

**하지만 시각적 도약(target jump 시 한 tick 안에 캐릭터가 끌려가는 현상)은 메커니즘적으로 동일하게 발생할 수 있다.** `PrepareRootMotion`(`cpp:933-1014`)의 force 산식 `Force = (CurrentTargetLocation - CurrentLocation) / MovementTickTime`(`cpp:957`)은 MoveToForce와 같다. lerp 시작점은 고정 `StartLocation`이고, 캐릭터 현재 위치는 force의 출발점일 뿐 lerp 곡선의 출발점이 아니다.

다만 GAS wrapper(`MoveToActorForce`)가 `TargetLerpSpeedHorizontalCurve/VerticalCurve`로 TargetLocation 변화율을 rate-limit하기 때문에(`cpp:248-276`), 적절히 튜닝하면 큰 도약 없이 부드럽게 따라간다. **즉 엔진의 정공법은 "TargetLocation rate-limit으로 도약 자체를 막는다"** 이고, 본 프로젝트가 한 "StartLocation 역산"은 같은 문제를 다른 각도에서 푸는 것이다.

### 5-2. HeightCurve 적용성

`GetPathOffsetInWorldSpace`(`cpp:919-931`)는 MoveToForce와 동일하게 동작:
- `FacingRotation = (TargetLocation - StartLocation).Rotation()` — TargetLocation이 변하면 facing rotation이 회전 → 활 모양 곡선의 옆 offset 방향이 회전.
- 이는 곡선의 정점(MoveFraction=0.5에서의 height)을 직접 흔들지는 않지만, target이 옆으로 크게 움직이면 곡선이 "방향을 트는" 모양이 된다.
- StartLocation은 고정이므로 곡선의 "기점"은 흔들리지 않는다. 이 점에서는 본 프로젝트의 StartLocation 역산이 곡선 모양을 더 안정시키는 효과가 있을 수 있다 (역산은 곡선 기점을 캐릭터가 실제 있는 곳으로 옮김 → 곡선 시작이 매번 갱신).

**평가: 동일하게 작동하지만 의미가 약간 다르다.** MoveToDynamicForce + TargetLerpSpeed 조합은 "큰 target jump를 막아서 활 모양을 유지", 본 프로젝트의 StartLocation 역산은 "활 모양을 매 tick 캐릭터 현재 위치 기준으로 다시 그림". 게임 디자인에 따라 어느 쪽이 더 자연스러운지가 갈린다.

### 5-3. dedi 안전성

MoveToDynamicForce는 `ConvertRootMotionServerIDsToLocalIDs`(`CharacterMovementComponent.cpp:11938-12027`)에서 ensure trip을 발생시키지 않는다 (12015번 줄의 Matches 비교가 위치를 보지 않음). 본 프로젝트가 MoveToForce에서 본 것과 동일한 충돌은 **구조적으로 회피된다**.

이게 MoveToDynamicForce가 도입된 정확한 동기이기도 하다. MoveToForce가 "한 번 발사하고 잊어도 되는 정적 target" 전제로 설계됐다면, MoveToDynamicForce는 "이동하는 target을 따라가는 lunge/charge" 전제 — Matches에서 위치를 빼고, UpdateStateFrom으로 매 tick 머지하는 디자인이 그 이유를 그대로 보여준다.

### 5-4. 종합 결론 — 대안 구현 비교

| 본 프로젝트의 구성 요소 | MoveToDynamicForce 기반에서 필요했을까 |
| --- | --- |
| **StartLocation 역산** | 부분적으로 불필요. ensure trip 회피용으로는 불필요 (MoveToDynamicForce가 구조적으로 회피). 곡선 모양 유지용으로는 디자인 선택 — `TargetLerpSpeedCurve`로 충분히 부드러우면 불필요, 더 정확한 곡선이 필요하면 여전히 의미 있음. |
| **매 tick / timer 기반 source 교체** | 불필요. `MoveToActorForce::TickTask`(`cpp:305-363`)는 source를 1회 등록(`SharedInitAndApply`)하고 매 tick `SetRootMotionTargetLocation`으로 in-place 갱신만 한다. source 교체 없이 진행도와 시간이 그대로 보존된다. |
| **LastSourceStartLocation / LastSourceTargetLocation 캐싱** | 부분적으로 불필요. source의 멤버 자체가 그 역할을 한다 (`StartLocation`, `InitialTargetLocation`, `TargetLocation`). 외부 캐싱은 디버깅/시각화 용도로만 의미 있음. |
| **TargetLerpSpeedCurve 같은 rate-limit 메커니즘** | (본 프로젝트 commit "수동 주기 추가: 점진적 변경"에 해당) 엔진의 `TargetLerpSpeedHorizontalCurve/VerticalCurve`가 같은 일을 한다. MoveToActorForce를 그대로 썼으면 자체 rate-limit 코드가 불필요. |

**대안 구현이 더 단순했을지:** 그렇다. MoveToActorForce 기반 lunge였다면:
- `ApplyRootMotionMoveToActorForce(... TargetActor=enemy, TargetLerpSpeedHorizontal/Vertical=Curve, PathOffsetCurve=ArcCurve, ...)` 한 줄 호출
- source 재생성 / StartLocation 역산 / 수동 rate-limit 코드 모두 불필요
- 단 trade-off: TargetActor의 정확한 위치를 따라가는 "actor magnet" 동작이 디자인 의도와 정확히 일치해야 함. 본 프로젝트의 lunge가 "전투 타이밍 보정/예측 위치 조정" 같은 커스텀 로직을 lunge 도중에 끼워야 한다면, GAS task의 고정된 `CalculateTargetOffset` 동작이 부족할 수 있다 (그 경우 MoveToDynamicForce를 raw로 쓰며 외부에서 SetTargetLocation 호출하는 형태가 된다).

---

## 6. 한 문장 결론

**MoveToDynamicForce 기반으로 lunge를 구현했다면 dedi 시뮬프록시 ensure trip은 발생하지 않았을 것**이며, 이는 `Matches()`가 TargetLocation을 비교하지 않고(`RootMotionSource.cpp:866-879`) `UpdateStateFrom`이 매 tick 위치를 머지하는(`cpp:896-910`, `CharacterMovementComponent.cpp:1949`) 설계 때문이다. **새 코드의 StartLocation 역산은 ensure trip 회피 목적으로는 불필요한 우회였을 가능성이 높고**, 곡선 모양을 캐릭터 실제 경로에 정렬시키는 부수적 효과는 디자인 선택의 영역이다 — 엔진의 정공법은 `TargetLerpSpeedCurve` 기반 rate-limit이며, 이 프로젝트의 "수동 주기 점진 변경" 코드와 동일한 문제를 푸는 엔진 표준 도구가 이미 GAS task wrapper(`AbilityTask_ApplyRootMotionMoveToActorForce`)에 포함돼 있다.

---

## 부록: 검증되지 않은 / 추적이 어려운 부분

- `MoveToActorForce`가 `TargetLocation`을 `COND_SimulatedOnly`로만 복제한다(`cpp:370`). autonomous proxy(예측 클라이언트)는 자기 쪽에서 직접 `UpdateTargetLocation`을 돌린다 — 이때 server와 client의 enemy 위치 인식이 다르면 task 단계에서 미세하게 갈릴 수 있다. 이것이 client-side prediction과 함께 작동할 때 어떤 시각적 결함을 야기하는지는 본 분석 범위 외.
- `bRestrictSpeedToExpected=true`일 때(`cpp:959-982`)의 동작은 force 크기를 expected speed에 맞춰 클램프하지만, target이 mid-flight에 변하는 경우 expected speed의 의미가 달라진다 — 이 분기의 lunge 적용 적합성은 디자인 의도 의존적.
- `ConvertRootMotionServerIDsToLocalIDs`의 mapping 생명주기(`CharacterMovementComponent.cpp:11941-11973`)와 `SavedMove` 캐시의 상호작용은 본 분석에서 깊이 들어가지 않았다. 본 프로젝트의 ensure trip 발생 시점이 정확히 이 매핑 어떤 단계에서 깨졌는지는 별도 추적이 필요하다 (보고서 본문에서는 12015번 줄 ensureMsgf만 인용).