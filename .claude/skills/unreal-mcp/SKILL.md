---
name: unreal-mcp
description: unreal-mcp(언리얼 에디터 MCP)를 쓰는 모든 작업의 공통 규칙과 함정. 에셋·BP·레벨·애님 그래프를 MCP로 편집하거나, 에디터를 실행·종료하거나, PIE·로그를 확인하거나, ap_toolset 툴을 추가·수정할 때 먼저 읽는다. add-enemy·edit-statetree·create-map 스킬의 MCP 공통 주의사항도 이 문서가 원본이다.
---

#unreal-mcp 공통 규칙과 함정

모두 실제로 겪은 문제에서 나온 규칙이다. 작업별 절차는 add-enemy(적 추가), edit-statetree(StateTree 편집), create-map(맵 제작) 스킬을 따른다.
반드시 지킬 BP 기본값 규칙은 CLAUDE.md "#MCP 에셋 편집 주의사항"에 있다.

##에디터 실행·종료와 빌드
-실행 전 `tasklist`로 UnrealEditor.exe가 이미 떠 있는지 확인한다. 사용자가 연 에디터가 있으면 새로 띄우지 않고 MCP 연결만 쓴다
  -중복 인스턴스가 에셋 파일을 잡아 저장이 실패한다(Windows 오류 32). MCP 포트(8000)는 먼저 뜬 인스턴스가 가져가서 수정은 원래 에디터에 들어가고 저장만 막힌다
-실행: `"D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "D:/unreal/ActionPractice/ActionPractice.uproject"`를 백그라운드로 띄우고 http://127.0.0.1:8000/mcp 응답을 기다린다
-빌드 후 에디터는 내가 MCP로 할 작업이 남았을 때만 실행한다 (CLAUDE.md #빌드)
-종료는 사용자에게 요청하는 것이 기본이다. 사용자가 세션 단위로 허락한 경우에만 정상 종료(`taskkill //IM UnrealEditor.exe`, /F 없이)를 요청한다. 저장 확인 창 등으로 멈추면 강제 종료하지 않고 사용자에게 알린다
-Claude Code 세션이 에디터보다 먼저 시작됐으면 MCP 연결이 실패(ECONNREFUSED) 상태로 남을 수 있다. 사용자에게 재연결을 요청한다
-C++ 함수 본문만 바꿨으면 에디터를 켠 채 LiveCodingToolset.CompileLiveCoding으로 반영한다. UCLASS/UFUNCTION/UPROPERTY 추가·시그니처 변경, Build.cs 변경은 에디터 종료 → 빌드 → 재실행이 필요하다. Live Coding 활성 중에는 UBT 빌드가 거부되므로, Live Coding으로만 반영한 변경은 다음 에디터 종료 때 정식 빌드로 마무리한다
-Live Coding 결과에 "data type changes may cause packaging to fail"이 나오면, 그 USTRUCT/UCLASS를 참조하는 에셋(StateTree 노드, DA 등)을 정식 빌드·재시작 전까지 편집·저장하지 않는다
  -임시 타입을 참조한 채 저장되어 재시작 후 노드가 None이 된 사례가 있다. 재시작 후 describe_state_tree에서 `<None>` 노드가 없는지 확인한다

##툴 호출
-`call_tool`은 toolset_name(예: `ap_toolset.toolsets.anim_montage.AnimMontageTools`)과 tool_name(접두사 없는 툴 이름)을 따로 넘긴다. 툴세트 이름은 list_toolsets, 인자 스키마는 describe_toolset으로 확인한다
-describe_toolset 결과가 큰 툴세트(BlueprintTools 등)는 파일로 저장된다. python으로 툴 이름과 인자 목록만 뽑아 본다
-스키마상 필수인 인자는 기본값이 있어 보여도 넘긴다
  -ap_toolset 툴에서 기본값이 빈 문자열('')인 인자
  -BlueprintTools.find_node_types의 context_pins (빈 배열)
  -SceneTools.find_actors의 name, tag, collision_channels
-자주 틀리는 인자 형식: ObjectTools.get_properties(instance, properties 배열), ObjectTools.set_properties(instance, values JSON 문자열), AssetTools.save_assets(asset_paths 배열)
-여러 툴 호출은 ProgrammaticToolset.execute_tool_script로 묶어 왕복을 줄인다. 샌드박스 스크립트의 dict는 .get(k, default)를 못 쓴다
-대량 에셋 로드 금지: 처음 여는 메시는 스태틱 메시·거리장 빌드가 동기로 돌아 에디터를 수 분~수십 분 막는다. 호출을 중단해도 에디터 쪽 스크립트는 끝까지 실행된다
  -조사는 실제로 쓸 에셋 몇 개만 소량씩 한다. 대량 조회가 필요하면 사용자에게 먼저 확인한다
  -진행 상황은 `Saved/Logs/ActionPractice.log`의 `Built static mesh [N s]` 줄로 확인한다

##프로퍼티 편집
-BP 기본값(CDO·컴포넌트 템플릿)은 BlueprintDefaultsTools.set_blueprint_defaults로만 바꾼다 (CLAUDE.md). 결과를 검증할 때는 배치 인스턴스 값까지 확인한 뒤 결론을 낸다
-배열: 크기 변경과 기존 원소 변경을 한 호출에 넣으면 "ArrayAdd/ArrayRemove: elements changed alongside the size change"로 거부된다
  -늘릴 때: get_properties로 읽은 기존 원소를 그대로 둔 채 새 원소만 붙인다. 기존 원소도 바꿔야 하면 같은 크기로 먼저 바꾼 뒤, 다시 읽은 값에 새 원소를 붙인다
  -줄이거나 통째로 바꿀 때: 빈 배열로 한 번 설정한 뒤 새 배열을 설정한다
  -빈 배열에 원소를 처음 채우는 것은 한 번에 된다
-ActorTools.set_actor_transform에 일부 필드만 넘기면 나머지가 0으로 초기화된다(툴 설명과 다름). 항상 get_actor_transform으로 읽은 location·rotation·scale 전체를 넘기고, 대량 변경 전에는 1개로 먼저 시험한다
-BP가 실제로 쓰는 에셋(Niagara, 머티리얼 등)은 컴포넌트 프로퍼티를 MCP로 읽어 확정한 뒤 수정한다. 바이너리 문자열 grep은 후보 탐색용이다 (쓰지 않는 에셋을 고친 사례)
-BP에 새 SceneComponent를 추가해 기존 컴포넌트를 그 아래로 옮길 때는 Mobility를 먼저 맞춘다. Static 자식은 Movable 부모에 부착이 거부되어 배치 인스턴스에서 월드 원점에 떨어진다
  -확인은 BlueprintDefaultsTools.describe_instance_components (ActorTools.get_parent_component는 BP 계층만 읽어 정상으로 보인다)
-꺼져 있는 설정(StateTree 상태·노드의 비활성, DA에서 비운 항목 등)은 사용자가 일부러 꺼 둔 것으로 본다. 버그 수정 수단으로 다시 켜지 말고, 왜 그 기능이 실행되는지(데이터·코드 경로)부터 추적해 "켜는 안"과 "실행 자체를 막는 안"을 함께 제시한다

##에셋 종류별 후처리
-MCP 수정은 자동 저장되지 않는다. AssetTools.save_assets로 저장한다
  -PIE 실행 중에는 save_assets가 "Asset does not exist"로 실패한다(실제로 있는 에셋도). 사용자가 PIE 중일 수 있으니 IsPIERunning을 확인하고, 끝난 뒤 다시 저장한다. 수정 내용은 메모리에 남아 있다
-StateTree: 편집 후 반드시 컴파일한다 (edit-statetree 스킬)
-블렌드스페이스: SampleData(샘플 추가·이동·삭제)를 바꾸면 AnimMontageTools.resample_blend_space를 호출한다. 리플렉션 편집은 블렌드 데이터를 재계산하지 않아 새 샘플이 무시된다
-BP·ABP 그래프: 편집 후 BlueprintTools.compile_blueprint(warnings_as_errors=True)로 경고까지 확인한다
-사운드: AudioTools(import_sound_waves → create_sound_cue → create_sound_asset)로 만든다. 웨이브 여러 개를 넘기면 Random 노드 Cue가 된다. SoundCue 기본 VolumeMultiplier가 0.75이므로 음량을 맞춘 파일이면 1.0으로 바꾼다. 감쇠·동시 재생 제한·볼륨은 ObjectTools.set_properties로 설정한다 (Concurrency는 maxCount와 platformMaxCount.default를 함께 설정)
-AnimNotify를 몽타주 0초에 두면 시작 프레임에서 건너뛸 수 있어 0.01초에 둔다. 몽타주의 Notifies 배열(TriggerChance 등 이벤트 필드)은 ObjectTools로 읽거나 쓸 수 없다. 사운드 빈도 조절은 SoundConcurrency의 retriggerTime으로 한다

##애님 그래프 (BlueprintTools)
-노드 타입 ID가 에디터 언어로 현지화돼 있다. find_node_types는 한국어로 검색한다
  -예: `Animation|SkeletalControls|스트라이드워핑`, `Animation|SkeletalControls|다리IK`, `애니메이션|스페이스변환|로컬을컴포넌트로`, `애니메이션|스페이스변환|컴포넌트에서로컬로`
-스켈레탈 컨트롤 노드는 컴포넌트 공간 포즈를 쓴다. 로컬↔컴포넌트 변환 노드는 핀 연결 시 자동으로 들어가지 않으므로 직접 만들어 연결한다
-노드 설정은 ObjectTools로 노드의 `Node` 구조체를 설정하고, 노출 핀의 기본값은 set_pin_value로 설정한다
-C++ AnimInstance 변수의 getter는 `Variables|<카테고리, 공백 제거>|Get<이름>`으로 만든다
-스테이트 머신 내부 서브그래프에서는 find_node_types가 실패한다(AnimStateNode → Blueprint 캐스트 오류). 최상위 AnimGraph에서는 동작한다

##툴 테스트와 백업
-툴 테스트로 실제 에셋을 바꿨으면 컴파일·저장 전에 원래 값으로 되돌린다. 실수로 저장했으면 즉시 복원하고 보고한다
-툴 동작 확인은 임시 복제본에서 하고 끝나면 지운다
-서로 참조하는 에셋이 든 폴더를 AssetTools.delete(폴더 경로)로 한 번에 지우지 않는다. DeleteDirectory → ForceDeleteObjects가 "사용 중" 모달 창을 띄운 뒤 에디터가 크래시했다(SoundCue + SoundWave 시험 폴더). 참조하는 쪽(Cue 등)부터 에셋 경로 단위로 하나씩 지우고 마지막에 폴더를 지운다
-execute_tool_script 안에서 execute_tool 오류는 try/except로 잡히지 않는다. 실패를 기대하는 시험 호출은 스크립트에 섞지 말고 단독 호출로 한다 (오류 뒤 호출이 실행됐는지도 결과로 판단할 수 없으니 상태를 다시 조회한다)
-백업은 AssetTools.duplicate로 `TEMP_` 접두사 사본을 만들고, 작업이 끝나면 사용자 확인 후 삭제한다. 이름 변경으로 백업하면 원래 경로에 리다이렉터가 남아 같은 이름으로 다시 만들 수 없다
-검증용 임시 액터는 `TEMP_` 접두사로 배치하고 레벨은 저장하지 않는다

##PIE와 로그
-사용자가 플레이 중일 수 있으므로 임의로 PIE를 종료하지 않는다
-내가 띄우는 PIE는 플레이어를 조작할 수 없다. 행동 선택 순서, 몽타주 재생 길이, 판정 결과 같은 로그 확인까지만 하고, 조작이 필요한 재현과 체감 품질(움직임, 미끄러짐)은 사용자에게 요청한다
-로그는 LogsToolset.GetLogEntries(카테고리·정규식 필터) 또는 `Saved/Logs/ActionPractice.log`에서 본다. 사용자가 "마지막 PIE 로그"라고 하면 마지막 PIE 시작 이후 구간을 찾는다
-원인이 불분명하거나 사용자와 내 추측이 엇갈리면, 추가 수정 대신 임시 측정 로그(구분 접두사 예: `[ChaseMeasure]`)를 넣고 재현을 요청한다. 원인을 확정한 뒤 로그를 지운다

##캡처
-EditorAppToolset.CaptureEditorImage / CaptureViewport 결과는 커서 파일로 저장된다. base64를 디코드해 scratchpad나 collab/에 이미지로 저장한 뒤 확인한다
-CaptureViewport는 annotations 인자가 필수다 (축소 보기 스크립트는 create-map 스킬 scripts/view.ps1)

##ap_toolset 툴 추가·수정
-Python 수정은 ProjectToolsetAdminTools.reload_project_toolsets로 반영한다. 새 툴세트 클래스는 `toolsets/__init__.py` 등록 목록에 추가한 뒤 리로드하면 등록된다
-C++ 래퍼(ActionPracticeEditor 모듈의 UActionPractice*Library) 추가·시그니처 변경은 에디터 종료 → 빌드 → 재실행이 필요하다
-툴 작성 원칙
  -실패 이유를 반환값으로 돌려준다. bool 반환 + out 파라미터는 Python에서 false일 때 None으로 축약되어 메시지가 사라진다
  -입력을 직접 검사한다. 엔진은 Float 파라미터에 "abc"를 넣어도 0으로 성공 처리한다
  -쓰기 전에 계산을 끝내 일부만 바뀐 에셋이 남지 않게 한다. 되돌리기가 안 되는 편집은 설명에 백업을 먼저 하라고 적는다
  -적용 후 다시 읽어 검증하고, 같은 입력으로 다시 실행해도 안전하게 만든다
  -툴 설명은 실제 동작과 맞춘다 (예: StateTreeEditTools.move_state의 Into는 첫 자식으로 넣는다)
-새 툴은 실제 에셋의 임시 복제본으로 검증하고 reviewer 검수를 거친 뒤, 사용법을 해당 작업 스킬에 반영한다
