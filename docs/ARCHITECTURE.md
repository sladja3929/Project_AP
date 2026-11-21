#아키텍처 설계 의도

이 문서는 코드만 읽어서는 파악하기 어려운 설계 의도를 시스템별로 기술한다.
CLAUDE.md에는 핵심 불변식만 한 줄로 두고, 해당 시스템을 수정하기 전에는
이 문서의 관련 섹션을 먼저 참조한다.

---

##캐릭터 계층 설계
-적 캐릭터는 BaseCharacter → EnemyCharacter → BossCharacter 3단 계층이며, EnemyCharacter가 적 공통 로직(히트디텍션, HP바, 리셋, EnemyDataAsset 참조)을 담당함
-BossCharacter는 보스 전용 연출(조우/이탈 Multicast, BGM, 화면 하단 보스 HP바)만 추가하는 얇은 계층임
-플레이어 캐릭터는 BaseCharacter → ActionPracticeCharacter 단일 계층

##데미지 파이프라인 설계
-BaseAbilitySystemComponent가 IDefensePolicy를 구현하며, 데미지 처리 흐름은 반드시 아래 순서를 따름:

1. OnDamaged — 피격 시작 진입점
2. CalculateAndSetAttributes — 수식 계산, 어트리뷰트 설정 (하한 클램프 없음, 오버플로 인코딩 허용)
3. HandleOnDamagedResolved — 리액션 판정 및 실행
4. ResetBreakGauges — 음수 게이지를 최대치로 리셋 (엘든 링 패턴: 브레이크 직후 즉시 리셋)

-EDefenseResult(None/Blocked/GuardBroken/Parried)는 HandleOnDamagedResolved에서 결정되어 GE 컨텍스트와 GameplayCue로 전달됨
-ActionPracticeGameplayEffectContext는 DamageType, PoiseDamage, DefenseResult, bUnparriable을 담는 커스텀 GE 컨텍스트
-아군 피격 제외는 데미지 파이프라인 이전의 히트 판정 단계에서 처리: UAttackTraceComponent::ValidateHit가 소유자별 IsFriendlyActor로 거르며, 적 공격(EnemyAttackComponent)은 다른 적 전체를 아군으로 봄. 플레이어 무기의 CapsuleOverlapComponent는 별도 구현이라 이 필터를 거치지 않음 (적이 이 방식을 쓰게 되면 아군 필터를 따로 추가해야 함)
-데미지·회복은 메타 어트리뷰트(IncomingDamage/IncomingHealing)로 들어와 파이프라인이 SetHealth로 직접 바꾸므로, Health 변화에 반응하는 로직(적 머리 위 HP바 등)은 GE가 바꾼 어트리뷰트가 아니라 PostGameplayEffectExecute 전후 값이나 OnRep으로 판단해야 함

##Attack 시스템 설계
-Attack는 단일 어빌리티가 아니라 AttackSequenceAbility 중심 구조로 동작함
-AttackSequenceAbility는 입력, 차지 상태, 콤보, 액션 리커버리, 버퍼 입력, 히트 디텍션을 통합 관리함
-Attack 타입은 Normal Attack, Charge Attack, Sprint Attack, Roll Attack, Charge Sprint Attack을 포함하는 구조
-핵심 구현 흐름은 AttackSequenceAbility + NormalAttackAbility + ChargeAttackAbility이며, 답변 시 이 구조를 우선 반영할 것
-무기별 공격 데이터는 WeaponDataAsset의 FTaggedAttackData 배열에서 태그로 식별됨
-적 공격 데이터는 EnemyDataAsset의 FEnemyTaggedAttackData에서 관리됨

##GameplayCue 설계
-3단 계층: APGameplayCueNotify_Instant(원샷 베이스) → APGameplayCueNotify_Impact(피격 전용, Surface×DefenseResult 분기) / APGameplayCueNotify_Duration(Duration GE 연동)
-ImpactResponseDataAsset이 EPhysicalSurface × EDefenseResult별 이펙트/사운드를 매핑
-BP 자식 클래스에서 프로퍼티만 세팅하면 코드 수정 없이 새 비주얼을 추가할 수 있는 구조
-캐릭터 사운드 경로는 재생 주체로 나눔: 몽타주·로코모션에 묶인 소리는 AnimNotify_PlaySound (몽타주가 클라이언트에 복제되어 각자 재생), 몽타주 없는 StateTree 상태의 소리는 Execute Gameplay Cue 태스크 → Instant 큐 (StateTree AI는 서버 전용이라 사운드를 직접 재생하면 데디 서버에서 클라이언트에 들리지 않음). 이동하는 대상의 울음은 Instant 큐의 bAttachSound로 소켓에 붙여, 소유자 기준 SoundConcurrency(한 마리당 울음 1개 등)가 적용되게 함

##UI 설계
-MasterHUDWidget이 중앙 HUD 컨테이너이며, PlayerController가 소유함
-3개 레이어(BaseLayer, WorldEventLayer, ModalLayer)로 위젯을 z-order 분리함
-위젯은 Character가 아닌 PlayerController가 관리하는 것이 원칙 (관심사 분리)
-타이틀 화면은 별도 GameMode/PlayerController 조합으로 분리됨

##상호작용 설계
-IInteractable 인터페이스 기반, InteractionComponent가 감지/실행을 담당
-Bonfire(휴식/리스폰)와 PickupItem(필드 아이템)이 IInteractable을 구현
-InteractionComponent → PlayerController → MasterHUDWidget 순으로 프롬프트 UI가 연결됨

##StateTree AI 설계
-GASStateTreeAIComponentSchema가 ASC를 StateTree Context에 노출하는 구조
-Evaluator가 런타임 데이터(체력 비율, 타깃 거리/각도)를 갱신하고, Task가 행동(어빌리티 활성화, GE 적용, 이동 등)을 실행함
-태그 기반 Condition은 노드 선택 시점에만 평가됨 — 글로벌 전환이 필요하면 Root 레벨 On Event Transition을 사용해야 함
-StateTree 이벤트 전송은 ASC에서 담당 (어빌리티가 아닌 ASC 레벨에서, GAS/ST 관심사 분리)
-목표 액터 추적 이동(MoveTo + TargetActor)은 경로를 0.5초마다·목표가 100uu 이상 움직였을 때만 재계산하는데, 가속 기반 경로 추종(bUseAccelerationForPaths)의 감속은 실제 목표가 아닌 이 오래된 경로 끝을 기준으로 함. 움직이는 플레이어를 쫓으면 경로 끝을 지날 때마다 감속·재가속이 반복되므로(움찔거림) 추적하는 적 CMC는 bUseFixedBrakingDistanceForPaths + FixedPathBrakingDistance 0으로 경로 끝 감속을 끔 (정지는 다음 공격 트랜지션이 담당)
-타깃 인지 상태와 전투 상태는 StateTree 노드가 아닌 EnemyAIController가 소유. 인지는 AIPerception 시야(시야각·SightRadius·가림) 그대로라 시야를 벗어나면 해제되고, 전투는 시야와 무관하게 타깃을 유지해 타깃 사망·LoseSightRadius 초과 수평 거리·자신의 사망/리셋으로만 해제됨 (엔진 시야 판정은 이미 본 대상에도 시야각을 적용하므로, 전투 중 등 뒤로 돌아간 플레이어를 놓치지 않으려면 별도 상태가 필요)
-전투 진입 시점은 StateTree가 판단: 전투 거리 조건을 통과한 상태(Combat, 늑대는 CombatEntryRange 안에서 진입하는 Howl)의 Enter Combat 태스크가 컨트롤러 전투 상태를 켬. 이 태스크는 계속 Running으로 남는 완료 판정 대상 태스크여야 함 (완료 판정에서 제외하면 이 태스크만 가진 Combat이 진입 즉시 완료 처리되어 자식 행동이 매 틱 재선택됨). 예외로 피격은 시야 밖이어도 적 ASC가 즉시 전투로 전환. ST는 Update Target Info Evaluator의 bInCombat으로 인지 단계 행동(늑대 Alert)과 전투 행동을 구분하고, 보스 조우 연출도 인지가 아닌 전투 상태 변경(OnCombatStateChanged)을 따름
-Idle 배회는 레벨 배치 액터 AEnemyWanderArea(박스 구역)를 적 인스턴스가 참조하는 방식(AEnemyCharacter::WanderArea, 없으면 스폰 위치 반경). 무리는 구역 하나를 공유해 한 영역에 묶이되 함께 움직이지는 않음. 순찰 루트 공유 대신 구역을 쓰는 이유는 같은 선 위를 오가는 구성원끼리 마주치기 때문. 겹침은 두 단계로 막음: 구역이 구성원별 목적지를 예약해 가까운 지점을 피하고(MinSeparation), 이동 중에는 CMC RVO 회피. Wander 태스크는 진행 방향 기준 각도·거리 범위에서 다음 지점을 골라 지그재그를 줄이고, 시작 대기를 랜덤화해 무리 출발을 분산
-전투 중 견제: 옆걸음 모션이 없는 사족 보행(Wolf)은 Circle Target 태스크로 타깃 둘레를 호를 따라 걷거나(몸은 이동 방향) Combat Idle 몽타주로 제자리 대기. 어느 쪽이든 몸 전체를 워프로 돌리지 않고 시선만 ABP 머리 바라보기가 타깃을 따라감. 실제 동물이 상대를 견제할 때의 움직임이라 strafe 모션을 만들지 않음. 선회 반경은 상위 상태 거리 범위 안쪽 여유(EdgeMargin)로 보정해, 완료 순간 경계를 넘어 다른 거리 상태(접근 등)가 잠깐 선택되는 끊김을 막음. 이동은 경로 이동(MoveTo)이 아닌 매 틱 이동 입력 조향(steering behavior): 현재 타깃 기준 LookAheadAngle 앞의 원 둘레 지점을 향해 이동 입력 방향을 TurnRate(도/초)씩만 돌림. 몸은 가속 방향을 따르므로, 경로를 주기적으로 재요청하면 가속 방향이 그때마다 꺾여 몸이 툭툭 돌지만 조향 방식은 타깃이 움직여도 측면이 타깃을 향한 채 부드럽게 따라 돎. 장애물은 주기적인 내비 레이캐스트로만 확인(막히면 반대 방향). 발 미끄러짐 보정은 아래 저속 보폭 보정 참고
-머리 바라보기: 적 ABP 공통 부모 UEnemyAnimInstance가 전투 대상(AEnemyCharacter::CombatTarget, 서버 전투 상태를 복제해 클라이언트에서도 사용) 쪽 수평 Yaw를 계산해 목·머리 본에 나눠 주고, ABP의 Transform (Modify) Bone 노드(Additive, Component Space)가 적용. 피치는 건드리지 않음(사족 보행 목 본 축이 위로 들려 있어 Look At으로 축을 맞추면 목이 꺾임). 몽타주 재생 중에는 Look At Target ANS(로직 없는 구간 표시) 구간에서만 허용하고, 그 밖의 몽타주 재생 중이거나 대상이 뒤쪽이면 정면으로 보간 복귀. ANS 활성 여부는 Begin/End 카운터 대신 AnimInstance의 활성 ANS 목록에서 조회해 몽타주 중단으로 End가 누락돼도 어긋나지 않음
-저속 보폭 보정(재생 속도 + Stride Warping): 걷기 블렌드스페이스 샘플 RateScale은 속도 / 애니메이션 기준 속도(디딤발이 지면에서 뒤로 움직이는 속도, 늑대 약 355)로 맞추되 하한 0.5를 둠. 재생만 늦추면 보폭 긴 걸음이 슬로모션처럼 보여 느린 선회(100)에서 미끄러지는 느낌이 남기 때문. 하한 아래 속도의 남는 이동량은 UEnemyAnimInstance가 StrideScale = 속도 / (기준 속도 × 재생 배율)로 계산해 ABP의 Stride Warping 노드(Manual 모드, 루트 모션 없는 제자리 애니메이션)가 보폭을 줄여 맞춤(늑대 100 → 재생 0.5·보폭 0.56). 기준 속도·재생 하한은 블렌드스페이스 값과 같아야 하며, 정지·출발 순간 튐을 막으려 StrideBlendInSpeed까지는 보정량을 서서히 올림. 노드 체인은 State Machine → Local To Component → Stride Warping → Leg IK → Component To Local → Slot으로, 몽타주(Slot 이후)에는 보정이 걸리지 않음. 스켈레톤에 IK 본이 없어 루트 기준 가상 본(VB ik_front_l 등)을 IK 발 본으로 추가하고, Leg IK가 FK 다리를 그 위치로 맞춤. 블렌드스페이스는 샘플 길이(길이/RateScale)를 가중 평균해 샘플 사이 재생 속도가 실제보다 느려지므로 샘플을 촘촘히 두고 실제 쓰는 속도(CircleSpeed·WalkSpeed)를 샘플 위치에 맞춤. 보정을 쓰는 적만 ABP 기본값 bEnableStrideWarping을 켬(꺼져 있으면 StrideScale 1 고정). 늑대는 저속에서 보폭을 줄여도 걷기 모션 자체가 어색해 선회 속도를 200으로 올리고(재생 하한 위라 보폭 보정 불필요) 보정은 꺼 둠. 노드 구성과 가상 본은 남겨 두어 다시 켜기만 하면 됨
-무리 전투: 조정자는 UEnemyPackSubsystem(서버 전용 WorldSubsystem), 무리 단위 상태는 UEnemyPack. 구성원 개별 전투 상태의 소유자는 여전히 EnemyAIController이고 무리는 소집·공격권·해제 판단만 담당. 사용 여부와 튜닝 값은 EnemyDataAsset::PackSettings(MaxAttackers, AttackerSwapMargin, RallyRadius)
  -소집: 무리 전투를 쓰는 적이 어떤 경로로든(Howl·근거리 진입·피격) EnterCombat하면 같은 배회 구역 구성원(구역이 없으면 RallyRadius 안 같은 EnemyName의 구역 없는 적)을 같은 타깃으로 EnterCombat시킴. 같은 타깃과 싸우는 무리가 있으면 합류. 소집된 구성원은 인지 단계(늑대 Alert·Howl)를 건너뛰고, 잠자던 구성원은 Rest 트랜지션의 In Combat 조건으로 Wake Up을 거쳐 합류. Howl 진입 조건이 비전투라 동시에 발견해도 먼저 전투에 들어간 한 마리만 Howl
  -공격권: 타깃에 가까운 순으로 MaxAttackers명. 빈 자리는 즉시 채우고, 기존 공격자는 비공격자가 AttackerSwapMargin 이상 더 가까울 때만 교체(거리가 비슷할 때 떨림 방지). 0.2초 주기와 구성원 변화 시 갱신. Update Target Info Evaluator의 bIsPackAttacker로 ST에 노출(무리 미소속이면 항상 true라 혼자 싸우는 적은 늘 공격자)
  -ST 분기: Combat 아래를 거리 구간이 아닌 역할로 먼저 나눔. Chase(ChaseRange 밖, 두 역할 공통 추격) → Attacker(bIsPackAttacker) → Support(그 외). Attacker는 Melee(근접 거리·정면) / Engage(Jump Attack·Approach 가중치 선택) / Turn To Target, Support는 Hold Ring(HoldRingMin~Max 고리에서 Circle·Combat Idle 가중치 선택) / Close In(고리 밖이면 고리까지 접근) / Give Way(고리 안쪽이면 바깥 호로 물러남)
  -분기·거리 규칙: 모든 적 공통의 Chase / Engage / Melee / Turn To Target 분기, 그룹 기준, 용도별 거리 파라미터는 [STATETREE_COMBAT_LAYOUT.md](STATETREE_COMBAT_LAYOUT.md)
  -역할 전환: 공격권은 진행 중인 공격 노드를 끊지 않고 끝난 뒤 재선택에만 반영. Support는 공격 노드가 없어 그룹 OnTick(bIsPackAttacker → Combat)으로 즉시 전환하고, Attacker는 이동 노드(Engage/Approach)에만 역할 변경 OnTick을 둠
  -해제: 무리 소속이면 거리 해제는 살아 있는 구성원 전원이 해제 거리 밖일 때만(한 마리만 멀어지면 무리 타깃을 유지해 다시 따라옴). 타깃 사망은 개별 판정. 전투 해제·사망·리셋·빙의 해제 시 컨트롤러가 무리에서 탈퇴하고 빈 무리는 제거
-몽타주 행동의 시작 각도는 StateTree가 판정: 정면이 필요한 행동 그룹·노드(Melee, Jump Attack) 진입 조건에 Target Angle 컨디션을 걸고(선회·제자리 대기처럼 정면이 필요 없는 견제 행동에는 걸지 않음. 선회 직후 몸이 옆을 향해 회전 몽타주가 끼어들기 때문), 각도를 벗어나 그룹이 선택되지 않으면 마지막 자식 Turn To Target(늑대는 Attacker의 마지막 자식)이 선택되어 회전 후 Combat 재선택. 행동 노드 안에서 회전하고 이어 재생하지 않는 이유는 회전 사이 거리·각도가 바뀌어도 ST가 처음부터 다시 고르게 하기 위함. 예외로 Alert는 실패 시 전투 추격 상태로 넘어가므로 그룹 안 첫 자식에 Turn을 둠
-시작 판정(ST)과 콤보 연계 판정(EnemyAttackAbility, FEnemyComboAttackUnit::MaxTargetAngle·MaxTargetDistance)은 서로 다른 판단이라 값이 중복되지 않음. 연계 값은 콤보 단계별 데이터라 단계를 모르는 ST에 둘 수 없음
-Turn To Target은 회전 몽타주가 있으면 TurnTarget 워프(Rotation Method = Scale)로 애니메이션의 회전량을 실제 각도에 맞추고, 없으면 이동 입력으로 걸으며 회전 (ABP는 속도로 Idle/Walk를 고르므로 제자리 회전은 Idle 재생 → 미끄러짐)
-몽타주 재생 경로는 두 가지로 구분: 히트 판정·쿨다운·데미지 등 게임플레이 데이터가 있으면 어빌리티(+EnemyDataAsset), 연출 전용(대기·경계·하울링 등)이면 PlayMontageTask가 어빌리티 없이 ASC::PlayMontage로 재생 (RepAnimMontageInfo로 복제, 데이터는 StateTree 노드에 둠)

##액션 및 입력 설계 의도
-Sprint: 홀드 방식
-Block: 홀드 방식, 입력 해제 대기와 피격 후 재활성화 흐름까지 고려할 것
-Parry: IA_SpecialAction 입력, ParryAbility는 ActionRecoveryAbility 상속, ParryWindow 커브로 State.Parrying 태그 제어
-ActionRecoveryAbility는 버퍼 입력 허용, 리커버리 상태 태그 관리, 버퍼 실행까지 담당함
-ChargeAttackAbility는 입력 유지/해제와 차지 시작 커브, 최대 차지, 노차지 분기를 포함함

##액션 회전 (Motion Warping) 설계
-몽타주 액션의 회전은 코드 틱 회전이 아닌 BaseCharacter의 APMotionWarpingComponent(UMotionWarpingComponent 상속)로 처리. 회전 구간/곡선은 몽타주의 MotionWarping ANS가 정의하고, 몽타주가 끊기면 워핑도 함께 종료됨
-워프 타깃 이름은 UAPMotionWarpingComponent 상수로 관리: Facing(지정 Yaw, Rotation Type=Default) / AttackTarget(타깃 추적, Rotation Type=Facing). 둘 다 Warp Translation off인 회전 전용
-이동 워프는 별도 타깃 AttackApproach(Warp Translation on, Warp Rotation off)로 분리. 타깃에서 오너 쪽으로 WarpStopDistance(EnemyComboAttackUnit)만큼 떨어진 지점을 VectorFromTargetToOwner 오프셋으로 추적하고, 회전은 같은 구간에 겹친 AttackTarget ANS가 담당 (정지 지점을 바라보면 도착 직전 방향이 불안정). 정지 지점은 매 틱 타깃→오너 방향으로 재계산되어 정지 거리 안에서는 오너 뒤에 생기므로, 공격 시작 시 이미 정지 거리 안이면 설정하지 않고 워프 도중 들어오면 감시 타이머가 제거 (남은 구간은 원래 루트 모션)
-회전 워프는 남은 구간 시간에 비례해 각도를 나누므로 구간이 짧을수록 빠르게 돎. 발이 땅에 붙은 정지 구간에서 돌면 몸이 긴 4족(Wolf)은 엉덩이가 옆으로 끌려 미끄러져 보이므로, 회전 ANS는 몸이 크게 움직이는 예비동작·공중 구간에 길게 두고 WarpMaxRotationRate로 구간 끝 스냅(타깃 이동으로 남은 각도를 마지막 프레임에 몰아서 회전)을 제한함
-애니메이션 이동량이 0인 구간(제자리 공격)도 SkewWarp가 AddTranslationEasingFunc 곡선으로 이동량을 생성. 타깃이 움직이면 매 틱 시작점이 재설정되므로 Linear가 아닌 곡선은 속도가 일정하지 않음
-복제: 엔진이 WarpTargets를 서버 → 시뮬레이티드 프록시로만 복제(COND_SimulatedOnly). 오너 클라이언트 → 서버 방향은 복제되지 않음
-플레이어(ActionRecoveryAbility 계열): 로컬 컨트롤 측이 락온/입력 방향으로 Facing 타깃 계산 → 로컬 적용 + Server RPC 전달(SetWarpTargetYawPredicted). 방향이 없으면 타깃 제거해 이전 액션 타깃 잔존 방지
-적(EnemyAttackAbility 계열): 서버가 매 콤보 시작 시 캐싱된 타깃을 bFollowComponent로 AttackTarget·AttackApproach에 설정 → 시뮬 프록시 자동 복제. 단 런지(SuperJump)는 비루트모션 몽타주라 워핑 불가 → TrackingTarget ANS의 틱 회전(RotateToTarget) + 목적지 이벤트 발행 유지
-워핑은 루트 모션을 수정하는 방식이므로 몽타주 애니메이션의 Root Motion이 켜져 있어야 하고, MOVE_None 상태에선 동작하지 않음 (Rest는 이 이유로 틱 회전 유지)
