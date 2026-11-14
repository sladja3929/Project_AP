---
name: add-enemy
description: 새 적 NPC(일반몹 또는 보스)를 추가하는 절차. 캐릭터 BP, EnemyDataAsset, 스탯·어빌리티 세트 DA, 공격 어빌리티, AI 컨트롤러, StateTree, 히트 소켓, 몽타주를 unreal-mcp로 만들고 연결한다. "적 추가", "몬스터/보스 만들어", "BP_XXX 적으로 만들어", Characters/<이름> 폴더의 메시·애니메이션으로 적을 구성해 달라는 요청에 사용한다.
---

#새 적 NPC 추가 절차

에셋 작업은 unreal-mcp(엔진 내장 MCP)로 수행한다. 프로젝트 전용 툴세트(Content/Python/ap_toolset)가 몽타주와 StateTree 편집·컴파일을 담당한다.
시스템 설계 의도는 docs/ARCHITECTURE.md의 캐릭터 계층·Attack 시스템·StateTree AI 섹션을 따른다.

##0단계: 요청 범위 확정
-일반몹인지 보스인지 확인한다. 명시가 없으면 일반몹으로 간주하고 보고 시 명시한다.
-몽타주 범위
  -필수: 사망, 피격, 기본 공격 1개
  -선택(요청 시에만): 그로기 3종, 추가 공격, 콤보, 런지(돌진) 공격, 원본 애니메이션의 루트모션 활용
-선택 항목은 요청이 없으면 만들지 않고, 결과 보고에 "미구성 항목"으로 남긴다.
-소스 에셋 위치 확인: /Game/Characters/<Name>/ 아래 Meshes(SK_*, SKEL_*, PHYS_*), Animations(A_*), ABP_<Name>

##1단계: 레퍼런스 선택과 복제
레퍼런스 적을 복제한 뒤 값을 교체한다. 빈 BP를 새로 만드는 것보다 컴포넌트 기본값(콜리전 프로필, HP바 위젯, 태그 등)이 보존되어 안전하다.

| 구분 | 레퍼런스 | 부모 클래스 | 공통 어빌리티 세트 |
|---|---|---|---|
| 일반몹 | Characters/Stickman (BP_StickMan) | EnemyCharacter | /Game/GAS/DA_AbilitySet_EnemyCommon |
| 보스 | Characters/WoodGiant (BP_WoodGiant) | BossCharacter | /Game/GAS/DA_AbilitySet_BossCommon |

복제 대상 (AssetTools.duplicate, 경로는 /Game/Characters/<Name>/)
-BP_<Name> ← 캐릭터 BP
-DA_<Name> ← EnemyDataAsset
-DA_Stats_<Name> ← CharacterStatsDataAsset
-DA_AbilitySet_<Name> ← 개별 어빌리티 세트
-GAS/BP_<Name>_Attack ← 기본 공격 어빌리티 (일반몹: Stickman/GAS/BP_StickMan_LeftAttack, EnemyAttackAbility 기반)
-BP_<Name>AIController ← AI 컨트롤러 (EnemyAIController 기반)
-ST_<Name> ← StateTree

복제 직후에는 모든 에셋이 레퍼런스 적을 가리키므로, 2~6단계에서 참조를 전부 교체한다.
7단계의 의존성 검사로 레퍼런스 적 경로가 남지 않았는지 확인한다.

##2단계: 캐릭터 BP 설정
CDO(`/Game/.../BP_<Name>.Default__BP_<Name>_C`)와 컴포넌트 템플릿을 ObjectTools.set_properties로 설정한다.

-CDO
  -EnemyData, CharacterStatsData: 새 DA
  -StartAbilitySetsData: [공통 세트, DA_AbilitySet_<Name>]. 보스는 BossCommon 사용(BossDeath 포함)
  -AIControllerClass: BP_<Name>AIController_C
  -Tags: ["Enemy"] 유지, AutoPossessAI는 PlacedInWorldOrSpawned 유지
  -보스 전용: EnemyName(하단 보스 HP바에 표시), BossBGM
-`:CharacterMesh0`
  -SkeletalMeshAsset, AnimClass(ABP_<Name>_C), RelativeScale3D
  -콜리전 프로필 HitDetectionPhysics는 레퍼런스 값 유지 (WeaponTrace/HitDetection 판정 대상)
-`:CollisionCylinder` 캡슐 크기와 메시 Z 오프셋
  -SkeletalMeshTools.get_bounds로 메시 크기를 확인해 정한다. origin.z ≈ boxExtent.z이면 피벗이 발밑이므로 메시 Z = -CapsuleHalfHeight
  -CapsuleHalfHeight ≥ CapsuleRadius 제약. 사족 보행처럼 긴 체형은 반경을 키우고 높이를 낮춘다
  -전방 축(Yaw)은 메시마다 다르므로 값은 넣되 뷰포트 확인 필요 항목으로 보고한다
-BlueprintTools.compile_blueprint 후 저장

##3단계: 히트 소켓과 락온 소켓
-EnemyDataAsset.hitSocketInfo의 {hitSocketName, hitSocketCount}는 메시 소켓 `<hitSocketName>_0 ~ _(N-1)`로 확장된다. 소켓은 끝단에서 몸쪽 순서로 배치 (예: Stickman HitS_HandR_0=middle_01_r, _1=hand_r, _2=lowerarm_r)
-소켓 이름 규칙: `HitS_<부위>_<번호>` (예: HitS_Jaw_0)
-LockOnSocket: 상체 중앙 본(spine 상단)에 추가
-SkeletalMeshTools.add_socket은 메시 단위이므로, 같은 스켈레톤의 메시 변형(털 유무, 색상 등)이 있으면 모두에 동일하게 추가한다
-본 이름은 SkeletalMeshTools.get_bone_names로 확인

##4단계: 데이터 에셋
###DA_Stats_<Name>
-attributeInitValues를 적 성격에 맞게 조정한다. 값을 정하지 못했으면 레퍼런스 값을 유지하고 "밸런싱 필요"로 보고한다

###DA_<Name> (EnemyDataAsset)
-hitSocketInfo, taggedAttackData, 몽타주 슬롯(death, hitReactionLight/Middle/Heavy, groggyStart/Loop/End)
-기본 공격 1개: attackTags=EnemyAbility.Attack.1, attackType=Normal, comboSequence 1개
  -usingSocketConfigs.socketName은 번호 없는 hitSocketName (예: HitS_Jaw)
-선택 몽타주를 만들지 않은 슬롯은 None으로 둔다. 레퍼런스 적 몽타주가 남지 않게 할 것
  -그로기 몽타주가 None이면 EnemyGroggyAbility가 각 Phase를 건너뛴다(루프 타이머는 유지). 크래시 없음

###DA_AbilitySet_<Name>
-grantedAbilities에 BP_<Name>_Attack (추가 공격 어빌리티도 여기에 등록)

###공격 어빌리티 BP
-일반 공격: EnemyAttackAbility 기반. AbilityTags(Asset Tag)가 DA의 attackTags와 일치해야 공격 데이터를 찾는다
-런지 공격(선택): EnemyLungeAbility 기반, 태그 EnemyAbility.Attack.Lunge.N, DA의 attackType=Lunge + lungeConfig.heightCurve (WoodGiant SuperJump 참고)
-태그 신설이 필요하면 GameplayTagsSubsystem 규칙을 따르고, add-ability 스킬 절차를 함께 확인한다

##5단계: AI 컨트롤러와 StateTree
-BP_<Name>AIController CDO의 `:GASStateTreeAIComponent`.stateTreeRef를 읽어 stateTree 경로만 ST_<Name>으로 바꿔 다시 설정한다
  -parameters(meleeRange, middleRange)도 여기 있다. 캡슐이 커졌으면 meleeRange를 늘린다
-ST_<Name> 에디터 데이터 수정
  -스키마: `ST_<Name>.ST_<Name>:StateTreeEditorData_0.GASStateTreeAIComponentSchema_0`의 contextActorClass, aIControllerClass
  -구조 파악은 StateTreeEditTools.describe_state_tree (스테이트 경로, 태스크/컨디션/트랜지션과 ID 출력)
  -ActivateAbilityTask: StateTreeEditTools.set_node_property(state_path, node_id, "AbilityToActivate", "/Game/.../BP_X.BP_X_C"). 노드 ID가 유지되어 프로퍼티 바인딩이 보존된다
  -여러 공격을 쓰려면 공격 상태를 늘리는 대신 기존 ST 구조를 먼저 확인하고 사용자에게 구성 방식을 확인한다
  -구조 변경(스테이트·노드·트랜지션 추가/복사), 프로퍼티 바인딩, 전역 파라미터는 edit-statetree 스킬 절차를 따른다. 새 노드의 바인딩은 사용자에게 넘기지 않고 직접 설정한다
-**반드시 StateTreeCompileTools.compile_state_tree(save=true) 실행**
  -에디터 데이터만 바꾸면 런타임 컴파일 데이터는 레퍼런스 적 스키마 그대로라 AI가 동작하지 않는다
  -컴파일 후 `ST_<Name>:GASStateTreeAIComponentSchema_0`(컴파일된 스키마)의 contextActorClass가 새 BP인지 확인한다

##6단계: 몽타주
AnimMontageTools로 만든다. 모든 몽타주는 slot_name='DefaultSlot'.

###필수
| 몽타주 | 원본 선택 기준 | 노티파이 |
|---|---|---|
| AM_<Name>_Death | 사망 애니메이션 | 없음 (사운드는 요청 시) |
| AM_<Name>_HitReact | 짧은 정면/몸통 피격. Light/Middle/Heavy 슬롯에 공용 연결 가능 | 없음 |
| AM_<Name>_Attack | 제자리 기본 공격 (루트모션 버전은 요청 시) | MotionWarping + HitDetection |

기본 공격 노티파이 (Stickman 기준값, 원본 길이 L에 비례 배치)
-MotionWarping: track '2', 0.0001 ~ L×0.1
  -add_notify_state 결과의 RootMotionModifier에 `/Script/MotionWarping.RootMotionModifier_SkewWarp` 클래스 경로를 넣어 인스턴스를 만든 뒤, 그 서브오브젝트에 WarpTargetName='AttackTarget', bWarpTranslation=false, bIgnoreZAxis=true, bWarpRotation=true, RotationType='Facing', RotationMethod='SlerpWithClampedRate', WarpMaxRotationRate=2000 설정
-HitDetection: track '1', `/Script/ActionPractice.AnimNotifyState_HitDetection`, L×0.225부터 L×0.335 동안
-타이밍은 애니메이션을 보지 않은 추정값이므로 "몽타주 에디터에서 타격 프레임 조정 필요"로 보고한다

###선택 (요청 시에만)
-그로기: Start / Loop / End 3종. 원본이 길면 몽타주 RateScale로 압축. 루프 지속 시간은 DA의 groggyLoopDuration이 결정
-콤보: comboSequence 항목 추가 + 연계 판정 지점에 AnimNotify_CheckCondition
-런지: AnimNotifyState_Lunging, AnimNotifyState_TrackingTarget 구간 배치 (WoodGiant SuperJump 몽타주를 list_notifies로 확인 후 따라 구성)
-레퍼런스 몽타주 구성은 AnimMontageTools.list_notifies로 조회해서 따른다
-루프 몽타주(대기·경계 등): set_montage_sections로 Start/Loop/End 섹션을 만들고 Loop의 next를 자기 자신으로 둔다. End 섹션은 next가 없어야 한다. get_montage_info의 sections로 결과 확인
-MotionWarping은 루트 모션에만 적용되므로 원본 시퀀스의 루트 모션이 꺼져 있으면 워프가 동작하지 않는다. get_root_motion_info로 루트 이동량을 확인해 켜도 캡슐이 움직이지 않는 제자리 애니메이션인지(회전 워프만 적용) 판단한다

###ABP 확인
-ABP_<Name> AnimGraph에 Slot('DefaultSlot') 노드가 출력 포즈 경로에 있는지 BlueprintTools.get_connected_subgraph(AnimGraphNode_Root_0)로 확인한다. 없으면 몽타주가 재생되지 않으므로 사용자에게 알린다

##7단계: 검증
-AssetTools.get_dependencies로 새 에셋 전체에 레퍼런스 적 경로가 남지 않았는지 확인
-AssetTools.save_assets로 저장 (MCP 수정은 자동 저장되지 않음)
-PIE 검증 (사용자가 원하거나 동작 확인이 필요할 때)
  -PlayerStart 근처에 SceneTools.add_to_scene_from_asset으로 임시 배치(이름 TEMP_ 접두사), 레벨은 저장하지 않는다
  -EditorAppToolset.StartPIE(warmupSeconds 10) 후 PIE 액터 경로 `/Game/Maps/UEDPIE_0_<Level>.<Level>:PersistentLevel.<액터명>`으로 AbilitySystemInspectorToolset(GetGrantedAbilities, GetAttributeValues, GetActiveTags) 확인
  -LogsToolset.GetLogEntries로 EnemyHitReaction / EnemyGroggy / EnemyAttack / Error / ensure 로그 확인
  -사용자가 PIE를 플레이 중일 수 있으므로 임의로 StopPIE 하지 않는다. 종료 후 임시 액터 제거
-결과 보고: 만든 에셋 표, 필수/선택 몽타주 구성 여부, 추정값(캡슐, Yaw, 노티파이 타이밍, 스탯, meleeRange) 목록

##MCP 툴 사용 주의사항
-ObjectTools.set_properties로 배열 크기와 원소를 동시에 바꾸면 "ArrayRemove: elements changed alongside the size change"로 실패한다. 빈 배열로 한 번 설정한 뒤 새 배열을 설정한다
-BP CDO·컴포넌트 템플릿을 ObjectTools.set_properties로 바꾸면 열려 있는 레벨에 이미 배치된 인스턴스에는 전파되지 않는다(레벨을 다시 열어야 반영). 바로 PIE로 확인해야 하면 배치 인스턴스(`<Level>:PersistentLevel.<액터>.<컴포넌트>`)에도 같은 값을 설정한다
-ap_toolset 툴에서 기본값이 빈 문자열('')인 인자는 스키마상 필수다. 생략하지 말고 명시한다
-여러 툴 호출은 ProgrammaticToolset.execute_tool_script로 묶어 왕복을 줄인다
-ap_toolset Python 수정 후에는 ProjectToolsetAdminTools.reload_project_toolsets로 다시 로드한다. 리로드는 이미 등록된 툴세트 클래스만 다시 등록하므로, 새 툴세트 클래스(파일) 추가는 에디터 재시작이 필요하다 (기존 클래스에 메서드 추가는 리로드로 반영)
-C++ 함수 본문만 바꿨으면 에디터를 켠 채 LiveCodingToolset.CompileLiveCoding으로 반영한다. UCLASS/UFUNCTION/UPROPERTY 추가·시그니처 변경, Build.cs 변경은 Live Coding으로 반영되지 않으므로 에디터 종료 → 빌드 → 재실행이 필요하다 (Live Coding 활성 중에는 UBT 빌드가 거부됨)
-에디터 실행은 `"D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "D:/unreal/ActionPractice/ActionPractice.uproject"`를 백그라운드로 띄우고 http://127.0.0.1:8000/mcp 응답을 기다린다
-CaptureEditorImage는 결과가 커서 파일로 저장된다. base64를 디코드해 collab/에 png로 저장한 뒤 확인한다
