# 2인 Co-op PvE Dedicated Server 구현 진행상황

## 완료된 작업 (2026-01-16)

### Phase 1: 프로젝트 설정 ✅
- ✅ `ActionPracticeServer.Target.cs` 생성 완료
- ✅ `ActionPractice.Build.cs` 수정: NetCore 모듈 추가
- ⚠️ Unity Build 비활성화 (`bUseUnity = false`) - linker 오류 해결 위해 추가

### Phase 2: Character Replication ✅
- ✅ `BaseCharacter.h/.cpp`: `bReplicates = true`, `GetLifetimeReplicatedProps` 구현
- ✅ `ActionPracticeCharacter.h/.cpp`:
  - `bIsLockOn`, `LockedOnTarget`, `LeftWeapon`, `RightWeapon` 복제 설정
  - `OnRep_LeftWeapon()`, `OnRep_RightWeapon()` 구현
  - `EquipWeapon()` 서버 권한 체크 추가
  - `GetLifetimeReplicatedProps` 구현
- ✅ `BossCharacter.h/.cpp`:
  - `GetLifetimeReplicatedProps` 구현
  - `BeginPlay()`의 AI 관련 코드에 `HasAuthority()` 체크 추가

### Phase 3: GAS 네트워크 설정 ✅
- ✅ `BossAbilitySystemComponent.cpp`: ReplicationMode를 Minimal로 변경
- ✅ `NormalAttackAbility.cpp`: NetExecutionPolicy를 ServerInitiated로 변경
- ✅ `ChargeAttackAbility.cpp`: NetExecutionPolicy를 ServerInitiated로 변경
- ✅ `HitReactionAbility.cpp`: NetExecutionPolicy를 ServerInitiated로 변경
- ✅ `EnemyAttackAbility.cpp`: NetExecutionPolicy를 ServerOnly로 변경
- ✅ `EnemyAttackAbility.h`: 생성자 선언 추가 (linker 오류 해결)

### Phase 4: 무기 및 히트 판정 ✅
- ✅ `Weapon.h/.cpp`:
  - `bReplicates = true`, `SetReplicateMovement(false)` 설정
  - `GetLifetimeReplicatedProps` 구현 (CalculatedDamage 복제)
  - `HandleWeaponHit()` 서버 권한 체크 추가
  - `#include "Net/UnrealNetwork.h"` 추가
- ✅ `AttackTraceComponent.cpp`: `PerformTrace()`에 `HasAuthority()` 체크 추가
- ✅ `WeaponCCDComponent.cpp`: `OnCapsuleBeginOverlap()`에 `HasAuthority()` 체크 추가

### Phase 5: GameMode 및 스폰 ✅
- ✅ `ActionPracticeGameMode.h/.cpp`:
  - `BeginPlay()`: 서버에서만 보스 스폰 (`HasAuthority()` 체크)
  - `ChoosePlayerStart_Implementation()`: 멀티플레이어 스폰 위치 분배 구현
  - `BossClass`, `BossSpawnLocation`, `BossSpawnRotation` 프로퍼티 추가

### 빌드 오류 해결 ✅
1. ✅ `bWithPushModel` 오류: Build.cs에서 제거 (Target.cs 속성임)
2. ✅ `DOREPLIFETIME` 식별자 오류: Weapon.cpp에 `#include "Net/UnrealNetwork.h"` 추가
3. ✅ Linker LNK2005 오류: EnemyAttackAbility.h에 생성자 선언 추가
4. ✅ Unity Build 충돌: Build.cs에 `bUseUnity = false` 추가

## 빌드 상태
**✅ 빌드 성공** (2026-01-16 19:01:58)
- 모든 Phase 완료
- 컴파일 및 링크 성공

---

## 다음 단계 (테스트)

### 1단계: Standalone 테스트
```
PIE Settings:
- Net Mode: Standalone
- Number of Players: 1
```
**목적**: 기존 싱글플레이어 기능이 정상 작동하는지 확인

**체크리스트**:
- [ ] 플레이어 스폰 정상
- [ ] 보스 스폰 정상
- [ ] 무기 장착/교체 정상
- [ ] 락온 기능 정상
- [ ] 공격 히트 판정 정상
- [ ] 데미지 적용 정상
- [ ] AI 동작 정상

### 2단계: 2인 Co-op 테스트
```
PIE Settings:
- Net Mode: Play As Client
- Number of Players: 2
- Run Dedicated Server: 체크
```

**체크리스트**:
- [ ] 2명의 플레이어가 다른 PlayerStart에 스폰
- [ ] 보스가 1마리만 스폰
- [ ] 각 플레이어의 무기 비주얼이 서로 보임
- [ ] 각 플레이어의 락온 상태가 복제됨
- [ ] 공격 히트 판정이 서버에서만 발생
- [ ] 양쪽 플레이어의 공격이 보스에게 데미지 적용
- [ ] 보스 체력바가 동기화됨
- [ ] 몽타주 애니메이션이 동기화됨

### 3단계: 네트워크 에뮬레이션 테스트
```
콘솔 명령어:
- Net PktLag=100
- Net PktLoss=5
```

**체크리스트**:
- [ ] 레이턴시 환경에서도 공격 예측 정상
- [ ] 패킷 손실 상황에서도 데미지 적용 정상
- [ ] 클라이언트 측 예측이 올바르게 동작

---

## 주요 변경사항 요약

### 네트워크 복제 설정
- 모든 Character 클래스: `bReplicates = true`
- Weapon: `bReplicates = true`, `SetReplicateMovement(false)`
- 무기 소켓 attachment는 `OnRep` 함수로 클라이언트에서 동기화

### GAS 네트워크 정책
- Player ASC: Mixed mode (기존 설정 유지)
- Boss ASC: Minimal mode (GameplayEffect만 복제)
- 공격 Abilities: ServerInitiated (서버에서 시작, 클라이언트 예측)
- AI Abilities: ServerOnly (서버 전용)
- 이동 Abilities: LocalPredicted (기존 설정 유지)

### 서버 권한
- 모든 히트 판정: `HasAuthority()` 체크
- 무기 장착: 서버에서만 실행
- 보스 스폰: 서버에서만 실행
- AI 로직: 서버에서만 실행

### 싱글플레이어 호환성
- `HasAuthority()`는 싱글플레이어에서 항상 `true` 반환
- 기존 동작에 영향 없음

---

## 파일 변경 이력

### 생성된 파일 (1개)
- `D:\unreal\ActionPractice\Source\ActionPracticeServer.Target.cs`

### 수정된 파일 (18개)
1. `ActionPractice.Build.cs`
2. `BaseCharacter.h`
3. `BaseCharacter.cpp`
4. `ActionPracticeCharacter.h`
5. `ActionPracticeCharacter.cpp`
6. `BossCharacter.h`
7. `BossCharacter.cpp`
8. `BossAbilitySystemComponent.cpp`
9. `NormalAttackAbility.cpp`
10. `ChargeAttackAbility.cpp`
11. `HitReactionAbility.cpp`
12. `EnemyAttackAbility.h`
13. `EnemyAttackAbility.cpp`
14. `Weapon.h`
15. `Weapon.cpp`
16. `AttackTraceComponent.cpp`
17. `WeaponCCDComponent.cpp`
18. `ActionPracticeGameMode.cpp`

---

## 트러블슈팅 메모

### Linker LNK2005 오류
**증상**: "public: __cdecl UEnemyAttackAbility::UEnemyAttackAbility(void)" 중복 정의

**원인**:
- cpp 파일에만 생성자를 구현하고 헤더에 선언하지 않음
- GENERATED_BODY()가 디폴트 생성자를 만들어서 충돌

**해결**:
- 헤더 파일에 `UEnemyAttackAbility();` 생성자 선언 추가
- Unity Build 비활성화 (`bUseUnity = false`)

---

## 참고사항

### Dedicated Server 빌드 방법
```bash
# Development 빌드
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ActionPracticeServer Development Win64 -Project="D:\unreal\ActionPractice\ActionPractice.uproject"

# Shipping 빌드
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ActionPracticeServer Shipping Win64 -Project="D:\unreal\ActionPractice\ActionPractice.uproject"
```

### PIE에서 Dedicated Server 실행
1. Editor Preferences > Play > Multiplayer Options
2. "Run Dedicated Server" 체크
3. "Number of Players" = 2
4. PIE 실행

---

## 원본 계획 문서
원본 계획은 다음 파일에 저장되어 있습니다:
`C:\Users\나원준\.claude\plans\peppy-twirling-puppy.md`
