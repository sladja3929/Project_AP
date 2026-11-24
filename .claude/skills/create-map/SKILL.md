---
name: create-map
description: 환경 에셋 팩(KiteDemo 등)으로 플레이용 맵을 unreal-mcp로 처음부터 만드는 절차. 지형 높이맵·레이어 생성, 랜드스케이프 머티리얼·그래스 타입, 폴리지 산포, 조명·포그, 적·상호작용 배치, 경계(투명 벽), NavMesh, 성능 최적화, 커밋 범위까지 다룬다. "맵 만들어", "새 레벨 구성", "숲/필드/보스룸 배치", "지형 새로", "맵 밝게/어둡게", "폴리지 밀도", "맵 최적화" 요청에 사용한다.
---

#맵 제작 절차

레퍼런스 결과물은 /Game/Maps/Lvl_DeepForest (KiteDemo 숲, 시작 → Wolf 필드 → StickMan 필드 → 휴식 → 보스룸).
레벨 편집은 프로젝트 툴세트 `ap_toolset.toolsets.level_edit.LevelEditTools`가 엔진 SceneTools를 보충한다.
- create_level: 빈 레벨(비 World Partition) 생성 후 열기. 현재 레벨이 저장돼 있어야 함
- create_landscape: .r16 높이맵 + 레이어별 .raw 웨이트맵으로 랜드스케이프 생성 (C++ UActionPracticeLandscapeEditLibrary 경유)
- create_foliage_type / create_landscape_grass_type: FoliageType·LandscapeGrassType 에셋 생성
- scatter_foliage: JSON 트랜스폼 목록을 폴리지 인스턴스로 배치 (폴리지 툴로 계속 편집 가능, Undo 1회로 되돌림)
- rebuild_navigation: Static NavMesh 재빌드
보조 스크립트는 이 스킬의 scripts/ 폴더에 있다 (gen_forest.py, deps.py, cap.py·shrink.ps1·view.ps1).

##1단계: 요구 정리와 에셋 파악
-사용자에게 확인: 용도(시연/정식), 구간 구성(필드 수, 적 종류·수, 보스룸), 분위기(밝기·시간대), 경계 방식, 에셋 팩
-에셋 팩은 정적 메시 위주인 팩을 우선한다. 스플라인 BP 기반 나무(ProceduralNaturePack)는 툴로 다루기 어렵다
-메시 크기는 StaticMeshTools.get_bounds로 확인 (인자 이름 mesh, 경로 '/Game/X/Name.Name'). 피벗이 중앙인 바위는 묻힘 깊이를 따로 준다
-**처음 쓰는 메시를 MCP로 대량 로드하지 말 것.** 첫 빌드가 메시당 수 분씩 걸리고, 중단해도 에디터 쪽 스크립트는 계속 돈다. 몇 개씩 나눠 로드한다
-텍스처 색은 CaptureAssetImage로 본다 (이름만 믿지 말 것. 'grass_w_rocks'가 실제로는 붉은 흙 반점 텍스처였음)

##2단계: 레이아웃 설계 (생성 스크립트)
-scripts/gen_forest.py를 Saved/MapLayout/<맵>/에 복사해 ROUTE(동선 노드: x, y, 바닥 높이, 반폭), CLEARINGS(공터: x, y, 높이, 반지름, 종류 1=공터 2=보스룸), GAMEPLAY(시작점·모닥불·적·랜드마크)를 고친다
-좌표: 미터, X=북, 505×505 정점(정점 간격 1m), 랜드스케이프 정점 (0,0) = 월드 (0,0). 시드 고정으로 재현 가능
-스크립트가 하는 일
  -동선·공터 래스터화 → 챔퍼 거리장 → 바닥 평탄 + 양옆 숲 사면 + 가장자리 능선 + 노이즈로 높이 합성
  -가장 가까운 바닥 높이와 사면 프로필은 블러해서 섞는다 (안 섞으면 바닥 구간 경계에서 맵 끝까지 직선 단차가 생김)
  -레이어 웨이트(Moss 기본, ForestFloor 낙엽, Path 흙길, Rock 급경사), 합 255로 정규화
  -폴리지 다트 스로잉: 길가(액자) → 숲 안쪽(고밀도) → 원경(동선 140m 이내만), 길가 덤불·고사리, 바위·쓰러진 나무
  -투명 벽: 바닥에서 22.37m 거리 등고선을 따라 12m 선분 (챔퍼 거리는 정수값이 흔하므로 등고선 값은 비정수로 둬야 고리가 끊기지 않음)
  -preview.png 평면도 출력 → 에디터 작업 전에 먼저 검토한다
-배치 후 적·모닥불 자리 3m 안에 충돌 폴리지가 없는지 JSON으로 검사한다

##3단계: 레벨 디자인 원칙
-길은 가운데를 비우고 양옆을 덤불·고사리로 채워 시선을 유도한다. 길가에는 수관이 넓은 나무를 둬 머리 위를 덮는다
-랜드마크(목적지 너머의 2배 이상 큰 나무 등)로 방향을 알려준다
-적은 진입로에서 미리 보이게 둔다 (등 뒤 기습 금지). 무리 전투 필드는 반경 19m 이상
-보스룸: 높은 전망 지점에서 공터를 내려다본 뒤 내리막으로 들어가는 일방향 진입. 둘레는 가파른 사면(Rock 레이어)
-페이싱: 휴식(모닥불) → 이동 → 전투 → 탐색 보상(곁가지 공간) → 전투 → 보스 직전 휴식(모닥불) → 보스
-어두운 숲 → 밝은 공터로 대비. 원경은 포그로 흐리게 (대기 원근)

##4단계: 에디터 조립 (MCP)
1. create_level('/Game/Maps/Lvl_X')
2. 머티리얼: MaterialTools로 LandscapeLayerBlend(색·노멀 2개), LandscapeLayerCoords(근거리 4, 원거리 23), 레이어별 근·원 텍스처 Lerp, Moss 틴트 VectorParameter(MossTint), LandscapeLayerSample + LandscapeGrassOutput
   -LandscapeGrassOutput.GrassTypes 배열은 기본 원소가 1개라 크기와 원소를 한 번에 바꾸면 거부된다. get_properties로 읽은 원소를 그대로 둔 채 새 원소만 붙인다
3. create_landscape_grass_type: Moss용(FieldGrass 350, Fern 6, 꽃 1.5~2), ForestFloor용(FieldGrass 90, Fern_03 6). 밀도 단위는 10m×10m당, 프로젝트 grass.DensityScale 0.4가 곱해진다
   -SM_DeadLeaves_Flat은 그래스로 쓰면 검은 판처럼 보이므로 쓰지 않는다
4. create_landscape(heightmap, layers JSON, material, layer_info_folder, 0, 0, 0) → 랜드스케이프에 set_properties {"bEnableNanite": true} (저장 시 Nanite 메시 빌드, 머티리얼 Nanite 플래그 자동 설정 → 머티리얼도 저장)
5. create_foliage_type(나무·바위 collision=True, 덤불·고사리 False) → scatter_foliage 종류별
6. 조명(Lighting 폴더): DirectionalLight(pitch -50, yaw 200, Intensity 32, Temperature 5000, VolumetricScatteringIntensity 2), SkyAtmosphere, SkyLight(실시간 캡처, 1.5), ExponentialHeightFog(Density 0.02, Falloff 0.12, 녹청 Inscattering, 볼류메트릭 ScatteringDistribution 0.75), PostProcessVolume(Unbound, AEM_Manual, Bias 8.5, Vignette 0.35)
   -길이 어두우면 태양 고도를 올리고(그림자 짧게) 스카이라이트를 올린다. 프로젝트 기본 노출은 고정(Bias 1)이라 맵 PPV가 필수
   -보스룸 너머 랜드마크 거목(높이 약 50m)이 태양 쪽에 있으면 공터 절반 이상이 그림자에 덮인다. 태양을 진입 방향 뒤쪽(보스 정면 조명)으로 돌리고 고도를 올린다 (DeepForest: pitch -50/yaw -160에서 그림자 62% → pitch -65/yaw 15에서 0%)
   -각도는 높이맵(height.r16)과 폴리지 JSON·랜드마크로 공터 샘플점에서 태양 방향 레이를 쏴 그림자 비율을 계산해 고른 뒤 캡처로 확인한다
7. 게임플레이: PlayerStart(+100), BP_Bonfire, BP_Beer(+30), BP_StaminaOrb(+80), BP_Wolf/BP_StickMan(+120), BP_WoodGiant(+300, scale 2, yaw 180), NavMeshBoundsVolume(scale = 크기m/2)
8. 투명 벽: /Engine/BasicShapes/Cube, scale (길이/100, 0.5, 40), 컴포넌트에 {"BodyInstance":{"CollisionProfileName":"InvisibleWall"},"bHiddenInGame":true,"CastShadow":false,"bVisible":false}. InvisibleWall은 Visibility를 무시해 AI 시야는 통과
9. rebuild_navigation → 범위가 0이면 빌드 진행 중이니 잠시 뒤 RecastNavMesh 바운드를 다시 확인 → AssetTools.save_assets
-아웃라이너 폴더: Lighting, Gameplay(/Enemies, /Interactables), Environment/Landmarks, Boundary/InvisibleWalls
-레이아웃을 바꿔 다시 만들 때: Gameplay·Environment/Landmarks·Boundary 폴더 액터와 Landscape_* 를 지우고 4·5·7~9를 반복 (폴리지 타입 설정은 에셋에 남음)

##5단계: 최적화 (빌드 전 기본 적용)
-원경 나무는 동선 140m 이내만 생성 (포그·능선 너머는 안 보임)
-나무: CullDistance Max 25000, WorldPositionOffsetDisableDistance 5000
-덤불·고사리: WPO 거리 2500, bVisibleInRayTracing false, bAffectDistanceFieldLighting false, bEnableDensityScaling true, 컬링 7000~9000 (Lumen HWRT 프로젝트라 레이 트레이싱 제외 효과가 큼)
-바위·소품: 컬링 10000~20000
-Nanite 랜드스케이프
-측정은 빌드 버전에서 stat unit / stat gpu / stat foliage

##확인 (캡처)
-CaptureViewport는 annotations 인자 필수, 결과가 커서 tool-results 파일로 저장된다 → scripts/view.ps1 <파일> <이름> <scratchpad>로 축소 JPG를 만들어 본다
-잔디는 카메라가 머물러야 생성되므로 위치를 옮긴 직후 캡처에는 안 보일 수 있다. 같은 위치를 한 번 더 찍는다
-확인 지점: 시작 지점, 각 필드 진입로, 보스룸 전망 지점, 상공 경사 시점
-PIE는 StartPIE(warmup 8초) → 로그 확인 → StopPIE. 실제 플레이 확인(전투 공간, 경계 이탈, 프레임)은 사용자에게 요청

##주의사항
-폴리지 인스턴스는 액터가 아니므로 위치 문제는 JSON 원본에서 찾는다 (ScreenCoordsToWorld로 좌표를 구해 대조)
-처음 여는 환경 팩은 메시 빌드가 길다. 규격 조사는 실제로 쓸 메시 몇 개만 소량씩 한다
-MCP 공통 규칙과 함정(set_actor_transform 전체 지정, find_actors 필수 인자, execute_tool_script 제약, 대량 에셋 로드, Live Coding, 캡처)은 unreal-mcp 스킬을 따른다

##커밋 범위
-서드파티 팩은 전체가 아니라 맵이 참조하는 파일만 커밋한다: python scripts/deps.py <맵> <맵 에셋 폴더> <팩 폴더> <목록 파일>
-Content/KiteDemo/는 .gitignore 대상이므로 목록 파일을 git add -f로 추가한다 (`git add -f --pathspec-from-file=<목록 파일>`). 새 KiteDemo 에셋을 쓰면 다시 실행해 빠진 파일을 추가한다
-용량이 크면(수 GB) 사용자에게 먼저 확인한다 (LFS 용량·라이선스)
-Saved/MapLayout은 git 무시 대상이므로 재생성이 필요한 생성 스크립트 사본은 이 스킬 scripts/에 반영한다
