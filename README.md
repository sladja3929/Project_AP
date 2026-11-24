# Project AP

1인 개발 언리얼 프로젝트로, 3D 액션 게임의 핵심 시스템과 기능들을 설계하여 역량을 키우고, 게임 개발에 AI를 활용하여 AI 활용 숙달을 목표로 합니다.

## 개발 목적
+ 3D RPG 액션 게임을 직접 구현하며 설계 역량을 키우고  장르의 특성을 이해
+ 언리얼의 다양한 기술들을 사용해보며 엔진에 숙달
+ AI 환경을 구축하여 게임 개발에서의 AI 활용을 숙달

## 시연 영상
[![Video Label](http://img.youtube.com/vi/5DyYNm1-bnw/0.jpg)](https://www.youtube.com/watch?v=5DyYNm1-bnw)

## 환경
+ 플랫폼: Windows PC
+ 엔진: Unreal 5.8
+ 언어: C++

## 구현 기능
+ GAS 기반 플레이어 액션
+ 입력 저장 시스템: Input Buffer
+ 공격 충돌 판정 기법: Trace Sweep
+ Data Driven Design
+ State Tree, GAS 기반 Enemy AI
+ Dedicated Server 환경 Co-op 전투 동기화
+ Unreal Mcp와 툴 확장을 통한 작업 환경 구축

## 설계 문서
+ [ARCHITECTURE.md](docs/ARCHITECTURE.md): 시스템별 설계 의도와 클래스 관계 (데미지 파이프라인, 공격 시퀀스, GameplayCue, UI, AI 등)
+ [STATETREE_COMBAT_LAYOUT.md](docs/STATETREE_COMBAT_LAYOUT.md): 적 AI StateTree 전투 상태 구성 규칙

## AI 개발 환경
Claude Code와 언리얼 에디터 MCP로 개발 환경을 구축했습니다. 설계·기준 수립과 플레이 검수는 직접 하고, 코드 작성·에셋 생성은 아래 지침과 툴을 통해 AI에게 맡깁니다.

### 프로젝트 지침
+ [CLAUDE.md](CLAUDE.md): 빌드 규칙, 코드 컨벤션, 설계 불변식, GAS·네트워크·루트 모션·StateTree 주의사항

### 작업 절차 (Skills)
+ [unreal-mcp](.claude/skills/unreal-mcp/SKILL.md): MCP로 에셋·BP·레벨을 다룰 때의 공통 규칙과 함정
+ [add-enemy](.claude/skills/add-enemy/SKILL.md): 적 NPC 추가 절차 (BP, 데이터 에셋, 어빌리티, AI 컨트롤러, StateTree, 몽타주)
+ [add-ability](.claude/skills/add-ability/SKILL.md): GAS 어빌리티 추가 절차 (부모 클래스 선택, 태스크 구성, 태그 흐름)
+ [edit-statetree](.claude/skills/edit-statetree/SKILL.md): StateTree 에셋 편집 절차
+ [create-map](.claude/skills/create-map/SKILL.md): 환경 에셋으로 맵 제작 절차 (지형, 폴리지, 조명, 최적화)

### 구현 / 검수 분리 (Agents)
+ [reviewer](.claude/agents/reviewer.md): 구현 의도 설명 없이 플랜 범위와 diff만으로 독립 검수
+ [developer](.claude/agents/developer.md): 설계가 확정된 기계적 대량 변경만 위임
+ [convention-check](.claude/hooks/convention-check.js): 파일 편집 직후 코드 컨벤션 자동 검사 훅

### 에디터 툴 확장
+ [ap_toolset](Content/Python/ap_toolset/toolsets): 내장 MCP에 없는 기능을 채우는 프로젝트 전용 툴셋 (StateTree 편집·컴파일, 몽타주·노티파이, BP 기본값 전파, 레벨 편집, 리타깃, 오디오, UI 에셋)
