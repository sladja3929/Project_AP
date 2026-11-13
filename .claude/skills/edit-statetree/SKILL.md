---
name: edit-statetree
description: 기존 StateTree 에셋(ST_*)을 unreal-mcp로 편집하는 절차. 스테이트·태스크·컨디션·트랜지션 추가/삭제/복사, 노드 프로퍼티 설정, 프로퍼티 바인딩, 전역 파라미터(Parameters) 추가·조정, 컴파일·저장을 다룬다. "StateTree 수정", "상태 추가", "트랜지션 바꿔", "바인딩 연결", "파라미터로 빼", AI 행동 흐름·이동속도·거리 조건 조정 요청에 사용한다.
---

#StateTree 편집 절차

편집은 프로젝트 툴세트 `ap_toolset.toolsets.state_tree_edit.StateTreeEditTools`와 `state_tree_compile.StateTreeCompileTools`로 한다.
백엔드는 에디터 모듈의 UActionPracticeStateTreeEditLibrary이며, 열린 StateTree 에디터와 Undo에 반영된다.
StateTree AI 설계(스키마, ASC 컨텍스트, 이벤트 전송)는 docs/ARCHITECTURE.md의 StateTree AI 섹션을 따른다.
태스크·컨디션·이밸류에이터 C++ 코드를 새로 만들거나 바꾸는 작업은 사용자 확인 후 진행한다 (에디터 툴링 수정은 바로 진행 가능).

##1단계: 구조 파악
-describe_state_tree로 스테이트 계층, 태스크·컨디션·트랜지션과 노드 ID를 본다
-스테이트는 이름 경로("Root/Combat/Long Ranged/Sprint")로 지정한다. 글로벌 노드(이밸류에이터·글로벌 태스크)는 state_path를 빈 문자열로 둔다
-describe_bindings(state_path)로 현재 바인딩을 본다 (빈 문자열이면 전체)
-노드 프로퍼티 값은 describe 출력에 없으므로, 필요하면 ObjectTools.get_properties로 에디터 데이터를 읽는다

##2단계: 구조 변경
-스테이트: add_state(State/Group/Linked/LinkedAsset/Subtree), remove_state, move_state(Before/After/Into), set_state_property(Name, Tag, SelectionBehavior, bEnabled 등)
-노드: add_node(Task/EnterCondition/TransitionCondition, C++ 구조체 경로 또는 BP 클래스 경로), remove_node
-트랜지션: add_transition(Trigger, EventTag, TransitionType, Target, Priority), set_transition_target
-복사: copy_state_from(하위 포함), copy_node_from(노드·트랜지션). 다른 트리에서도 가능
  -노드 ID는 새로 발급되고, 복사 범위 내부의 트랜지션·바인딩은 새 ID로 다시 연결된다
  -복사 범위 밖을 가리키는 GotoState 트랜지션은 set_transition_target으로 재지정한다
  -같은 설정의 노드가 이미 있으면 새로 add_node 하기보다 copy_node_from이 값까지 가져와 빠르다
-SelectionBehavior는 자식 선택 방식이다. 무작위 선택이면 TrySelectChildrenAtRandom, 조건 기반이면 각 자식의 EnterCondition으로 거른다
-태그 기반 Condition은 선택 시점에만 평가된다. 실행 중 전환이 필요하면 상위 스테이트의 OnEvent/OnTick 트랜지션을 쓴다

##3단계: 노드 프로퍼티
-set_node_property(state_path, node_id, property_path, value_text)
  -값은 언리얼 텍스트 포맷 ("300", "True", "/Game/.../BP_X.BP_X_C", 구조체는 "(A=1,B=2)")
  -인스턴스 데이터를 먼저 찾고 없으면 노드 구조체에서 찾는다. 중첩은 '.'로 지정
  -트랜지션 ID를 주면 트랜지션 자체 필드(Priority, bDelayTransition, DelayDuration 등)를 설정한다
-노드 ID는 프로퍼티 변경으로 바뀌지 않으므로 기존 바인딩이 유지된다

##4단계: 바인딩과 파라미터
-튜닝 값(거리, 속도, 시간 등)은 노드에 직접 쓰지 말고 전역 파라미터로 만들어 바인딩한다. 디버거 표시나 다른 노드와 값을 공유하기 쉽다
-파라미터: set_root_parameter(name, value, type)
  -타입: Bool, Byte, Int32, Int64, Float, Double, Name, String, Text. 이미 있으면 타입은 무시하고 값만 바꾼다
  -잘못된 형식·범위의 값은 거부되고 아무것도 바뀌지 않는다
  -삭제는 remove_root_parameter. 바인딩에서 쓰는 중이면 사용처를 담아 거부되므로 바인딩부터 지운다
  -AI 컨트롤러 BP의 StateTreeRef.parameters에 같은 이름의 오버라이드가 있으면 그 값이 우선한다. 값을 바꿀 때 오버라이드 여부를 함께 확인한다
-바인딩 순서
  1. get_binding_sources(state_path, node_id)로 대상 노드에서 접근 가능한 소스 이름·섹션·ID와 프로퍼티를 본다
  2. set_property_binding(state_path, node_id, target_property, source_struct, source_property)
     -source_struct는 출력된 이름("Parameters", "Update Target Info Evaluator", "EnemyAIController")을 쓴다
     -같은 이름이 여럿이면 source_property가 해석되는 소스만 후보로 남는다. 그래도 여럿이면 에러에 나온 "이름 [섹션]" 또는 ID로 지정한다
     -source_property가 빈 문자열이면 소스 구조체·컨텍스트 객체를 통째로 바인딩한다 (컨텍스트 액터·컨트롤러·ASC 연결)
     -타입 호환을 검사한다. bool→double처럼 엔진이 승격을 허용하는 조합은 통과한다
  3. describe_bindings로 결과를 확인한다
-바인딩 해제는 remove_property_binding (하위 경로 바인딩 포함)
-컴파일 메시지의 "expected to have a binding" 목록이 남은 필수 바인딩이다

##5단계: 컴파일과 저장
-**편집 후 반드시 StateTreeCompileTools.compile_state_tree(save=true)**. 에디터 데이터만 바꾸면 런타임 데이터는 이전 그대로라 AI에 반영되지 않는다
-결과의 succeeded와 messages를 확인한다. 비활성 태스크 경고처럼 기존부터 있던 메시지는 변경 전후를 비교해 구분한다
-needs_recompile로 저장 상태와 편집 상태가 일치하는지 확인할 수 있다

##검증과 보고
-런타임 확인은 게임플레이 디버거(Apostrophe 키)로 사용자가 PIE에서 활성 스테이트를 본다
-보고: 바뀐 스테이트·노드(ID), 추가한 파라미터와 값의 근거, 바인딩 목록, 컴파일 결과, 사용자가 PIE에서 확인할 항목

##주의사항
-툴 테스트로 실제 트리를 바꿨다면 컴파일·저장 전에 원래 값으로 되돌린다. 실수로 저장했으면 즉시 복원 후 재컴파일하고 보고한다
-에셋 백업이 필요하면 AssetTools.duplicate로 TEMP_ 접두사 사본을 만들고, 작업이 끝나면 사용자 확인 후 삭제한다
-MCP 공통 주의사항(툴세트 리로드, Live Coding, 에디터 실행, 빈 문자열 인자)은 add-enemy 스킬의 "MCP 툴 사용 주의사항"을 따른다
