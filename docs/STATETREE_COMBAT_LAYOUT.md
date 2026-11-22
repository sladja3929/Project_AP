# 적 StateTree 전투 분기 규칙

적 StateTree(ST_*)의 `Root/Combat` 하위를 구성하는 공통 규칙이다.
새 적을 추가하거나 기존 적에 행동을 넣을 때 이 규칙에 맞춰 배치한다.
StateTree AI 전반(스키마, 인지·전투 상태 소유, 이벤트 전송)은 [ARCHITECTURE.md](ARCHITECTURE.md)의 StateTree AI 절을 따른다.

## 1. 표준 분기: Chase / Engage / Melee / Turn To Target

`Root/Combat`은 위에서부터 차례로 고르는 방식(TrySelectChildrenInOrder)이다. 자식은 아래 네 가지 역할로만 나눈다.

| 분기 | 담당 | 진입 조건 | 빠져나오는 조건 |
|---|---|---|---|
| **Chase** | 행동 거리 밖의 추격 | `d > ChaseRange` | OnTick `d <= ChaseRange` → Combat |
| **Engage** | 근접 거리 밖에서 하는 행동 (원거리 공격·접근·견제) | `d > MeleeRange` (+ 필요하면 정면 각도) | 각 행동이 완료되거나 자체 OnTick으로 재선택 |
| **Melee** | 근접 공격 | `d <= MeleeRange`, 정면 각도 | 공격 완료 후 재선택 |
| **Turn To Target** | 각도가 맞지 않아 위의 어느 분기도 고를 수 없을 때의 회전 | 정면 각도 밖 | OnStateCompleted → Combat |

- `d`는 Update Target Info Evaluator의 `DistanceToTarget`(수평 거리)이다.
- **Chase는 항상 맨 앞에 둔다.** Engage에는 하한 조건만 있다. 상한(`d <= ChaseRange`)은 Chase가 먼저 선택되는 순서가 대신한다. Chase 앞에 다른 상태를 끼우면 Engage가 먼 거리에서도 선택된다.
- **Engage와 Melee는 순서가 바뀌어도 된다.** 거리 조건이 겹치지 않는다(`d > MeleeRange` / `d <= MeleeRange`).
- **Turn To Target은 맨 뒤에 둔다.** 정면이 필요한 분기가 각도 때문에 선택되지 않았을 때만 도달해야 한다. 회전 후에는 Combat을 처음부터 다시 고른다. 행동 노드 안에서 회전한 뒤 이어서 재생하지 않는 이유는 ARCHITECTURE.md의 시작 각도 항목에 있다.
- Engage의 각 행동 사용 거리(예: 점프 공격 300~700)는 그 행동 노드의 진입 조건에 직접 건다. 조건이 거짓이면 그 행동은 후보에서 빠지고 형제 중에서 다시 고른다.

### 무리 적 확장 (Wolf)

무리 전투를 쓰는 적은 Chase 다음을 공격권(`bIsPackAttacker`)으로 한 번 더 나눈다.

```
Combat
├─ Chase
├─ Attacker   (bIsPackAttacker == true)   Melee / Engage / Turn To Target  ← 표준 분기와 같음
└─ Support    (bIsPackAttacker == false)  Hold Ring / Close In / Give Way  ← 위치 유지만
```

- **Support**: 공격 노드가 없다. 그래서 그룹 OnTick(`bIsPackAttacker == true` → Combat)으로 역할 변경을 바로 반영한다.
- **Attacker**: 진행 중인 공격을 끊지 않도록 이동 노드(Engage/Approach)에만 역할 변경 OnTick을 둔다.
- 무리에 속하지 않은 적은 `bIsPackAttacker`가 항상 true다. 그래서 혼자 있는 늑대는 표준 분기와 똑같이 동작한다.
- 공격권 결정 방식은 ARCHITECTURE.md의 무리 전투 항목을 따른다.

## 2. 그룹을 만드는 기준

상태를 그룹(자식을 가진 상태)으로 묶는 경우는 둘뿐이다.

1. **자식 여럿이 같은 진입 조건이나 태스크를 공유할 때**
   - 예: Melee 아래의 여러 공격이 근접 거리·정면 조건을 공유한다.
   - 예: Wolf Chase는 Run Attack·Approach To Target 형제와 BackJumpWeight 초기화 태스크를 공유한다.
2. **형제끼리 가중치로 골라야 할 때**
   - Combat은 Melee → Turn 우선순위를 지키려고 순서 선택 방식이다. 그래서 가중치 선택은 별도의 TrySelectChildrenAtRandomWeightedByUtility 그룹이 맡는다.
   - 예: Engage(Jump Attack·Approach, Strafe·Fast Approach·Approach)
   - 가중치 그룹의 자식에는 모두 Consideration을 둔다. Consideration이 없는 자식은 점수가 0이라 선택되지 않는다.

자식이 하나뿐인 그룹은 두지 않는다.
- 예: Stickman Chase는 그룹 없이 단일 상태다.
- 자식 하나뿐인 그룹은 이름만 늘리고, 거리가 다른 형제가 생기면 결국 다시 쪼개야 한다.

## 3. 거리 파라미터

### 용도별로 나눈다

| 종류 | 의미 | 파라미터 예 |
|---|---|---|
| 행동 사용 거리 | 이 거리에서 이 행동을 쓸 수 있다 | `MeleeRange`, `JumpAttackMinRange`/`MaxRange`, `SuperJumpMinRange` |
| 위치 유지 거리 | 이 거리에 머물거나, 이 거리까지 다가간다 | `ChaseRange`, `HoldRingMin`/`Max`, `StrafeMinRange`, `CombatEntryRange` |
| 인지 거리 | 전투 전 경계 범위 | `AlertRange` |

- Near/Middle/Long 같은 **공유 구간 파라미터를 쓰지 않는다.**
  - 구간 하나가 사용 거리와 위치 유지 거리를 겸하면, 한쪽을 조정할 때 다른 쪽도 함께 바뀐다.
  - 예: 비공격자 고리를 넓히려고 MiddleRange를 올리면 점프 공격 거리까지 늘어난다.
- Range1/Range2 같은 **번호 구간도 쓰지 않는다.** 트리나 디버거에서 무엇을 위한 거리인지 알 수 없고, 사이에 새 거리를 끼우면 번호를 다시 매겨야 한다.
- 이름은 `<행동/용도><Min|Max>Range` 형태로 짓는다. 하나만 있으면 Min/Max를 생략한다.
  - 예: `MeleeRange`, `ChaseRange`

### 값을 두는 위치

- 거리는 노드에 숫자로 직접 쓰지 않는다. ST 전역 파라미터(`Parameters`)로 만들어 바인딩한다.
  - 같은 값을 여러 노드가 공유할 수 있다.
  - 게임플레이 디버거가 이름으로 링을 그린다.
- AI 컨트롤러 BP의 `StateTreeRef.parameters`는 `propertyOverrides`에 이름이 있을 때만 ST 기본값보다 우선한다. 거리 조정은 보통 ST 쪽 기본값에서 한다.
- 콤보 연계 판정(EnemyAttackAbility의 `FEnemyComboAttackUnit::MaxTargetDistance`)은 시작 판정과 별개의 데이터다. 이 값은 ST 파라미터로 옮기지 않는다.

### 경계 떨림 방지

- 진입과 이탈 조건은 같은 파라미터로 대칭을 맞춘다.
  - 예: Chase는 `d > ChaseRange`로 진입하고 `d <= ChaseRange`에서 이탈한다.
  - 이렇게 하면 경계에서 두 상태가 번갈아 선택되지 않는다.
- 범위 안을 도는 이동(Circle Target)은 반경을 범위 안쪽 여유(EdgeMargin)만큼 좁혀 잡는다. 완료 순간에 경계를 넘어 다른 상태가 잠깐 선택되는 일을 막는다.

## 4. 현재 적용 현황

| 적 | Combat 하위 구성 | 거리 파라미터 |
|---|---|---|
| Stickman | Chase(단일) / Engage[Strafe·Fast Approach·Approach] / Melee / Turn To Target | MeleeRange 110, ChaseRange 600, StrafeMinRange 300, StrafeDistance 600(Strafe 반경) |
| WoodGiant | Engage[Approach To Target·Lunge: Super Jump·Ranged: Rock Throw] / Melee / Turn To Target | MeleeRange 150, SuperJumpMinRange 800 (Rock Throw도 같은 거리 사용) |
| Wolf | Chase[Run Attack(비활성)·Approach To Target] / Attacker[Melee·Engage·Turn To Target] / Support[Hold Ring·Close In·Give Way] | MeleeRange 190, JumpAttackMinRange 300, JumpAttackMaxRange 700, HoldRingMin 700, HoldRingMax 1000, ChaseRange 1300, CombatEntryRange 1300, AlertRange 1800 |

- **WoodGiant에는 Chase가 없다.** 근접 거리 밖은 모두 Engage가 맡는다. 그래서 Engage의 상한이 없다.
- **WoodGiant의 첫 원거리 행동은 항상 Super Jump(개막 패턴)다.** ApproachUtility·RockThrowUtility가 0으로 시작하고, Super Jump가 끝날 때 Set Float Parameter로 JumpUtility 값을 넣는다. 그 뒤로 SuperJumpMinRange 밖에서는 Approach To Target·Super Jump·Rock Throw를 같은 비중으로 고르며, 원거리 공격은 Super Jump와 Rock Throw 중 하나가 된다.
- **Stickman의 StrafeDistance와 ChaseRange는 용도가 다르다.** 앞의 것은 Strafe 반경, 뒤의 것은 추격 경계다. 지금은 값이 같지만, Strafe 반경을 ChaseRange보다 크게 잡으면 Strafe 중 Chase 경계를 넘나든다.

## 5. 디버깅

게임플레이 디버거(Apostrophe 키)의 EnemyAI 카테고리가 위 파라미터를 이름으로 찾아 링으로 그린다(`GameplayDebuggerCategory_EnemyAI.cpp`의 `RangeEntries`). 해당 ST에 없는 파라미터는 건너뛴다.

| 색 | 종류 |
|---|---|
| 빨강 | 근접 공격 거리 |
| 주황 | 원거리 공격 사용 거리 |
| 초록 | 위치 유지 거리 |
| 노랑 | 추격·전투 진입 |
| 자홍 | 인지 |

새 거리 파라미터를 추가하면 `RangeEntries`에도 해당 색으로 등록한다.
