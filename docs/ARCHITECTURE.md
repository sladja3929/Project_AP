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
-몽타주 행동의 시작 각도는 StateTree가 판정: 행동 그룹(Middle Ranged, Melee) 진입 조건에 Target Angle 컨디션을 걸고, 각도를 벗어나 그룹이 선택되지 않으면 Combat 마지막 자식 Turn To Target이 선택되어 회전 후 Combat 재선택. 행동 노드 안에서 회전하고 이어 재생하지 않는 이유는 회전 사이 거리·각도가 바뀌어도 ST가 처음부터 다시 고르게 하기 위함. 예외로 Alert는 실패 시 Long Ranged로 넘어가므로 그룹 안 첫 자식에 Turn을 둠
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
