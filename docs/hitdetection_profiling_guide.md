# 히트디텍션 성능 비교 측정 가이드 (AttackTrace vs CapsuleOverlap)

적응형 스윕(`AttackTraceComponent`)과 일반 캡슐 오버랩(`CapsuleOverlapComponent`)의
성능/판정 비용을 **같은 스윙**에서 비교하기 위한 실측 절차서.

측정은 **2계층**으로 한다.

- **A. 헤드라인 지표 — 윈도우당 쿼리 횟수** (로그만으로 측정, Insights 불필요, 제일 쉬움)
- **B. 시간 지표 — 씬쿼리 시간 A/B 직접 비교** (Unreal Insights + 북마크 구간)

먼저 A를 확실히 뽑고, 시간까지 필요하면 B로 간다.

---

## 0. 사전 준비 (측정 신뢰도 3대 조건)

측정 전에 아래 3가지를 반드시 맞춘다. 하나라도 어기면 숫자가 흔들린다.

| #   | 조건                 | 이유                                            | 방법                         |
| --- | ------------------ | --------------------------------------------- | -------------------------- |
| 1   | **디버그 드로잉 전부 OFF** | `DrawDebugCapsule`/스윕 드로잉 비용이 측정 윈도우에 그대로 섞임  | 아래 "드로잉 끄기" 참고             |
| 2   | **프레임레이트 고정**      | 오버랩 갱신 횟수(M)가 프레임 의존이라 캡이 없으면 실행마다 달라짐        | 콘솔에 `t.MaxFPS 60`          |
| 3   | **스탠드얼론 실행**       | 히트 판정이 서버 전용(HasAuthority)이라 PIE 클라이언트에선 안 잡힘 | Play 모드를 **Standalone** 으로 |

### 드로잉 끄기

- 게임 중 키보드 **`1`** 한 번으로 **AttackTrace 드로잉 + Overlap 드로잉이 함께** 켜지고 꺼진다.
  화면에 `[Player] Attack Draw: OFF` 가 뜨면 두 기법 모두 드로잉 OFF 상태.
- 무기 장착 시 기본값이 OFF(`bWeaponDebugTrace=false`)로 적용되므로, 보통은 그대로 두면 된다.
  드로잉을 켜서 궤적을 눈으로 보고 싶을 때만 키 1을 누른다.

### 로그 활성화 확인 (이미 켜둠)

카운터 로그는 아래 두 파일에서 `#define ENABLE_DEBUG_LOG 1` 이어야 출력된다. (현재 1로 설정됨)

- `Source/ActionPractice/Private/Characters/HitDetection/AttackTraceComponent.cpp`
- `Source/ActionPractice/Private/Characters/HitDetection/CapsuleOverlapComponent.cpp`

측정이 끝나면 로그 비용을 없애기 위해 다시 `0`으로 돌려도 된다. (단 그러면 카운터 로그도 사라짐)

---

## A. 헤드라인 측정 — 윈도우당 쿼리 횟수

**핵심 논지**: "적응형 트레이스는 스윙 속도에 따라 쿼리 수를 조절한다"를 숫자로 직접 보여준다.

- AttackTrace = 윈도우당 **스윕 횟수 N** (주기·보간 수에 따라 가변)
- CapsuleOverlap = 윈도우당 **오버랩 갱신 프레임 수 M** (프레임 의존, 조절 불가)

### 측정할 로그 (Output Log에서 확인)

| 기법             | 로그 카테고리                      | 메시지                                             | 언제 찍히나        |
| -------------- | ---------------------------- | ----------------------------------------------- | ------------- |
| AttackTrace    | `LogAttackTraceComponent`    | `Stopped trace, counter: N`                     | 스윙(트레이스) 종료 시 |
| CapsuleOverlap | `LogCapsuleOverlapComponent` | `HitDetection Ended - Overlap update frames: M` | 히트 윈도우 종료 시   |

### 절차

1. 에디터에서 **Standalone** 으로 실행.
2. 콘솔(`` ` `` 키) 열고 `t.MaxFPS 60` 입력. → 조건 2 충족.
3. 드로잉 OFF 확인 (위 0번). → 조건 1 충족.
4. **Output Log** 창을 띄워둔다 (Window → Output Log). 필터 검색창에 `counter` 또는 `Overlap update` 입력해두면 편하다.
5. **AttackTrace 모드**로 맞춘다: 키 `3` 눌러 화면 메시지가 `HitDetection: AttackTrace` 인지 확인.
6. **동일한 공격 모션**을 여러 번(예: 10회) 반복 입력. 매 스윙마다 `Stopped trace, counter: N` 이 찍힌다. → N 값들 기록.
7. 키 `3` 눌러 `HitDetection: CapsuleOverlap` 로 전환.
   - ⚠️ 공격 중(`State.Ability.Attacking`)에는 토글이 무시된다. 화면에 사유 메시지가 뜨면, 공격이 끝난 뒤 다시 누른다.
8. **같은 공격 모션**을 같은 횟수만큼 반복. 매 스윙마다 `Overlap update frames: M` 이 찍힌다. → M 값들 기록.
9. 빠른 공격 / 느린 공격(차지 등) 모션별로 5~8 반복 → 스윙 속도에 따른 N 변화를 보여주면 논지가 선명해진다.

### 기록 템플릿

| 모션     | 스윙 속도 | AttackTrace N (평균) | CapsuleOverlap M (평균) | N/M 비 |
| ------ | ----- | ------------------ | --------------------- | ----- |
| 약공격 1타 | 빠름    |                    |                       |       |
| 강공격    | 느림    |                    |                       |       |
| 차지공격   | 매우 느림 |                    |                       |       |

> 해석 포인트: 오버랩 M은 모션 길이(프레임 수)에 거의 비례해 결정되는 반면,
> AttackTrace N은 스윙이 빠를수록 주기를 촘촘히/보간을 잘게 가져가며 **필요한 만큼만** 쿼리한다.

---

## B. 시간 측정 — Unreal Insights (씬쿼리 A/B 직접 비교)

**핵심 교정(중요)**: `P1 − P0` 같은 차분식은 쓰지 않는다.
스윕도 씬쿼리라서 Trace 모드의 물리 시간(P0)에 이미 스윕 비용이 포함돼 있어,
차분하면 오버랩 비용을 체계적으로 과소평가한다.
대신 **모드 간 "윈도우 내 씬쿼리 시간"을 그냥 직접 비교**한다.
공통 노이즈(캐릭터 이동 스윕, AI 트레이스)는 양쪽에 동일하게 끼므로 **N회 평균**으로 눌린다.
절대값(각 기법이 몇 ms인가)이 꼭 필요할 때만 "감지를 완전히 끈 동일 스윙"을 baseline으로 별도 측정한다.

### B-1. 트레이스 켜서 실행하기

측정 구간을 자동 마킹하도록 **북마크 채널**을 포함해 트레이스한다.
코드에 이미 `HitDetectionWindow_Begin` / `HitDetectionWindow_End` 북마크가
`AnimNotifyState_HitDetection`(두 모드 공용 지점)에 박혀 있다.

**방법 1 — Standalone 실행 인자 (권장, 깔끔함)**

1. **Edit → Editor Preferences → Level Editor → Play** 로 이동.

2. **Additional Launch Parameters** 에 입력:
   
   ```
   -trace=cpu,frame,bookmark
   ```

3. 이후 **Standalone** 으로 Play 하면 자동으로 트레이스가 기록된다.

**방법 2 — 에디터 트레이스 버튼 (PIE에서 즉석)**

1. 에디터 우하단 상태바의 **Trace(Insights) 아이콘** 클릭.
2. 채널에서 **CPU / Frame / Bookmark** 활성화 → **Start Tracing**.
3. 측정 후 **Stop Tracing**.

> 어느 방법이든 `cpu`, `frame`, `bookmark` 세 채널이 켜져 있어야 한다.
> `bookmark`이 빠지면 윈도우 마커가 안 보이고, `cpu`가 빠지면 타이머가 안 잡힌다.

### B-2. 측정 실행

1. 위 A절과 동일하게 조건 3대(드로잉 OFF, `t.MaxFPS 60`, Standalone)를 맞춘다.
2. **AttackTrace 모드**에서 동일 공격 모션을 10회 이상 반복.
3. 트레이스 저장 후(또는 계속 기록 상태에서) 키 `3` 으로 **CapsuleOverlap 모드** 전환, 같은 모션 10회 이상.
   - 두 모드를 **한 트레이스 안**에 담아도 되고(북마크로 구분됨), 세션을 나눠도 된다.

### B-3. Unreal Insights에서 읽기

1. **Tools → Run Unreal Insights** (또는 `Engine/Binaries/Win64/UnrealInsights.exe`) 실행.
   - 라이브 세션이면 자동 연결, 저장 파일이면 `.utrace` 열기.
2. **Timing Insights** 뷰를 연다.
3. 타임라인 상단에 **북마크 마커**(`HitDetectionWindow_Begin`/`_End`)가 세로선으로 표시된다.
   한 스윙의 Begin~End **시간 범위를 드래그 선택**한다.
4. 하단 **Timers** 탭을 연다. 선택 구간 안의 모든 타이밍 이벤트가 Count/Incl/Excl 로 집계된다.
5. **씬쿼리 계열 타이머 이름을 실측으로 확인**한다.
   - 필터 검색창에 `query`, `collision`, `sweep`, `overlap`, `physics` `GeomSweep`/`Overlap`/`Raycast`등을 넣어보고,
     **선택 구간에서 실제로 Count가 0이 아닌** 타이머를 찾는다.
   - ⚠️ 정확한 이름은 엔진 버전/물리 백엔드에 따라 다르다(레거시 Collision 그룹 vs Chaos 계열).
     이름을 추측하지 말고 **여기서 실제로 잡히는 이름을 확정**한 뒤, 두 모드에 **같은 타이머**를 쓴다.
6. 그 타이머의 **Inclusive 시간**을 AttackTrace 윈도우 / CapsuleOverlap 윈도우에서 각각 읽어 비교.
7. 노이즈가 크므로 **여러 스윙 구간을 반복 측정해 평균**낸다.

### 기록 템플릿

| 모드             | 윈도우 내 씬쿼리 타이머(이름: ____) Incl (평균, ms) | 비고  |
| -------------- | ------------------------------------- | --- |
| AttackTrace    |                                       |     |
| CapsuleOverlap |                                       |     |

### (선택) 절대값용 baseline

"각 기법이 몇 ms인가"가 필요하면:

- 동일 스윙을 **감지 완전 off** 상태(무기 미장착 또는 히트 윈도우 없는 모션)로 측정 → `P_base`.
- 각 모드 씬쿼리 시간에서 `P_base` 를 빼면 순수 감지 비용.
- 상대 비교(어느 쪽이 싼가)만 필요하면 이 단계는 불필요하다.

---

## C. 결과 서술(포트폴리오) 프레이밍

- 기존 `FEngineLoop::Tick` 전체 측정치(예: **10.77 vs 11.07 ms**)는 버리지 말고
  **"전체 프레임 관점에서는 차이가 노이즈 수준"** 이라는 맥락 문장으로 남긴다.
  → 이게 "그래서 더 좁은 단위(히트 윈도우)로 파고들었다"는 서사의 도입부가 된다.
- 그 다음 **A(쿼리 횟수)** 로 "적응형 트레이스가 쿼리 수를 능동 조절한다"를 숫자로 제시.
- 마지막에 **B(씬쿼리 시간)** 로 실제 비용 차이를 보강.
- 결론 톤: *"오버랩은 감지를 물리 씬 쿼리에 offload하고, 적응형 트레이스는 게임 스레드에서
  스윙 속도에 맞춰 쿼리 수를 직접 조절한다"* — 두 기법의 트레이드오프를 대비.

---

## D. 빠른 체크리스트

측정 시작 직전 한 번에 확인:

- [ ] Standalone 실행 (PIE 아님)
- [ ] `t.MaxFPS 60` 입력함
- [ ] 두 기법 드로잉 OFF (키 1 한 번, 화면 `Attack Draw: OFF`)
- [ ] `ENABLE_DEBUG_LOG 1` 로 빌드된 상태 (카운터 로그용)
- [ ] (B만 해당) `-trace=cpu,frame,bookmark` 또는 트레이스 버튼에서 3채널 ON
- [ ] Output Log 필터 `counter` / `Overlap update` 걸어둠
- [ ] 매 모드 전환 시 화면 메시지로 현재 모드 확인 (`HitDetection: AttackTrace` / `CapsuleOverlap`)
- [ ] 공격 중 토글 무시되면 공격 끝나고 다시 키 3

---

## 참고: 관련 코드 위치

| 항목                   | 위치                                                                                        |
| -------------------- | ----------------------------------------------------------------------------------------- |
| 전략 토글(키 3)           | `WeaponManagerComponent.cpp` `ToggleHitDetectionStrategy`                                 |
| 드로잉 토글(키 1, 두 기법 공통) | `WeaponManagerComponent.cpp` `ToggleWeaponDebugTrace` → `ApplyDebugTraceToCurrentWeapons` |
| 스윕 카운터               | `AttackTraceComponent.h` `DebugSweepTraceCounter`, 로그는 `StopTrace`                        |
| 오버랩 갱신 카운터           | `CapsuleOverlapComponent.h` `DebugOverlapUpdateCounter`, 로그는 `HandleHitDetectionEnd`      |
| 윈도우 북마크              | `AnimNotifyState_HitDetection.cpp` `NotifyBegin`/`NotifyEnd`                              |
