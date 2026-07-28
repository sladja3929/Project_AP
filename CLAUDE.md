#프로젝트 개요
-언어: 한국어로 소통
-Unreal Engine 5.7 C++ 프로젝트
-클래스 API를 제공할 때 무조건 언리얼 5.7 버전에서 유효한지 확인할 것
-엘든 링, 몬스터 헌터 월드를 레퍼런스로 하는 액션 RPG 게임 개발
-취업 포트폴리오 용 토이 프로젝트
-현업에서 자주 사용하는 기술들을 제공할 것
-프로젝트는 Game, Editor, Dedicated Server 타깃을 모두 고려할 것

#빌드
-빌드는 반드시 아래 명령 한 가지 형태로만 실행할 것 (git bash 기준, 경로/인자 변형 금지 — 권한 프롬프트 난립 방지)
-"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" ActionPracticeEditor Win64 Development "D:/unreal/ActionPractice/ActionPractice.uproject" -waitmutex
-데디케이티드 서버 타깃 빌드가 필요하면 타깃명만 ActionPracticeServer로 바꾸고 나머지 형태는 동일하게 유지할 것
-cmd.exe 래퍼, 백슬래시 경로(C:\...), UnrealBuildTool.exe 직접 호출 등 다른 형태로 조합하지 말 것

#코드 컨벤션
-중괄호는 Allman 스타일
-주석은 //와 텍스트를 붙일 것 -> "//주석 내용"
-if / for / while / switch 문 전후로 빈 줄 한 줄
-UPROPERTY 포인터 멤버는 로우 포인터보다 TObjectPtr 같은 언리얼 제공 포인터를 상황에 맞게 사용할 것
-웬만하면 클래스 생성자가 아닌 헤더 선언에서 멤버변수를 초기화할 것
-cpp 파일이 길어 구역을 나눌 때에는 #pragma region을 활용할 것
-cpp 파일은 생성자+초기화 함수를 맨 위, 소멸자+종료 함수를 맨 끝에 배치할 것(#pragma region 사용 X)
-헤더 파일에서 클래스를 선언할 때에는 아래 형식을 따를 것

class MyClass
{
public:
#pragma region "Public Variables"

> 퍼블릭 변수들

#pragma endregion

#pragma region "Public Functions"

> 퍼블릭 함수들

#pragma endregion

protected:
#pragma region "Protected Variables"

> 프로텍트 변수들

#pragma endregion

#pragma region "Protected Functions"

> 프로텍트 함수들

#pragma endregion

private:
#pragma region "Private Variables"

> 프라이빗 변수들

#pragma endregion

#pragma region "Private Functions"

> 프라이빗 함수들

#pragma endregion
}

-cpp 파일을 생성할 때는 아래 내용을 추가할 것

#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
    DEFINE_LOG_CATEGORY_STATIC(Log현재클래스이름, Log, All);
#define DEBUG_LOG(Format, ...) UE_LOG(Log현재클래스이름, Warning, Format, ##__VA_ARGS__)
#else
#define DEBUG_LOG(Format, ...)
#endif

#핵심 기술 스택
-GamePlay Ability System (GAS)
-State Tree
-Dedicated Server
-Enhanced Input
-UMG UI
-Gameplay Tags 기반 이벤트 처리
-애니메이션 레이어 인터페이스
-AnimNotify / AnimNotifyState 기반 전투 이벤트 전달
-Niagara VFX, GameplayCue
-Physics Asset 기반 충돌 판정
-네트워크 복제 및 로컬 예측 태그 처리
-데이터 에셋 기반 무기 / 아이템 / 입력 / 태그 / 적 / 이펙트 관리
-에디터 모듈 (커스텀 디테일 패널)

#설계 의도 핵심 불변식
아래는 매번 지켜야 하는 핵심 계약만 한 줄로 둔다. 시스템별 상세 설계 의도와
클래스 관계는 시스템을 수정하기 전 반드시 docs/ARCHITECTURE.md를 참조할 것.

-캐릭터 계층: 적은 BaseCharacter → EnemyCharacter → BossCharacter, 플레이어는 BaseCharacter → ActionPracticeCharacter
-데미지 파이프라인 순서는 반드시 OnDamaged → CalculateAndSetAttributes → HandleOnDamagedResolved → ResetBreakGauges (순서·관심사 고정)
-EDefenseResult는 HandleOnDamagedResolved에서 결정, GE 컨텍스트(ActionPracticeGameplayEffectContext)와 GameplayCue로 전달
-Attack는 단일 어빌리티가 아니라 AttackSequenceAbility 중심 구조 (입력/차지/콤보/리커버리/버퍼/히트디텍션 통합)
-GameplayCue는 3단 계층(Instant 베이스 → Impact / Duration), ImpactResponseDataAsset이 Surface×DefenseResult 매핑
-UI는 PlayerController가 소유하는 MasterHUDWidget 컨테이너 + 3레이어 z-order (위젯은 Character가 아닌 PC가 관리)
-상호작용은 IInteractable + InteractionComponent, 프롬프트는 InteractionComponent → PlayerController → MasterHUDWidget
-StateTree AI는 GASStateTreeAIComponentSchema로 ASC를 Context에 노출, 이벤트 전송은 어빌리티가 아닌 ASC 레벨에서
-입력: Sprint/Block은 홀드, Parry는 IA_SpecialAction (상세 흐름은 ARCHITECTURE.md)

#새 어빌리티 추가
-새 어빌리티/전투 액션을 추가할 때는 add-ability 스킬의 절차(부모 클래스 선택 → 태스크 구성 → 태그 흐름)를 따를 것
-직접 구현 요청이 없으면 부모 클래스 선택, 상속 이유, 태스크 구성, 태그 흐름을 먼저 설명할 것

#태그 및 이벤트 규칙
-GameplayTagsSubsystem은 프로젝트 전반의 태그 접근 진입점으로 간주할 것
-태그 관련 답변은 GameplayTagsSubsystem의 Static Getter 패턴과 Internal Getter 패턴을 기준으로 설명할 것
-입력, 상태, 어빌리티, 이벤트, 노티파이 태그가 전투 흐름 전반에 연결되어 있다고 가정할 것
-AnimNotify / AnimNotifyState는 Gameplay Event를 ASC로 전달하는 흐름으로 설명할 것
-버퍼 입력, 액션 리커버리, 차지 시작, 무적 시작, 콤보 리셋, 회전, 조건 체크 같은 이벤트는 태그 기반 흐름과 함께 설명할 것

#GAS 핵심 주의사항
-BlockAbilitiesWithTag는 CanActivateAbility만 게이트함. 이미 활성화된 어빌리티 내부(WaitGameplayEvent 등)에는 영향 없음
-Periodic GE의 Modifier는 Spec 생성 시점에 magnitude를 캐싱함 — 런타임 어트리뷰트 읽기는 반드시 Execute_Implementation 안에서 GetNumericAttribute로 해야 함
-CommitAbility는 Cost+Cooldown을 동시 처리함. 스태미나만 부분 소비하는 경우 CommitCooldown만 단독 사용
-CooldownDuration은 OnGiveAbility에서 주입하여 CommitAbility의 ApplyCooldown보다 먼저 세팅되어야 함
-ShortDurationTagManager는 GE Duration 1초 제한을 타이머 기반으로 우회하는 시스템

#네트워크/복제 주의사항
-OnPossess는 서버 전용. 클라이언트에서 캐릭터 캐싱은 AcknowledgePossession + OnRep_Pawn 오버라이드로 처리
-CMC 회전 플래그(bOrientRotationToMovement, bUseControllerDesiredRotation)는 자동 복제되지 않음 — Server RPC 필요
-FInputModeUIOnly는 레벨 전환 후에도 Slate 뷰포트 레벨에서 유지됨
-SetPause()는 의도적으로 사용하지 않음 (모든 클라이언트 일괄 정지 방지)

#충돌/히트디텍션 주의사항
-SweepMultiByChannel은 Block으로 설정된 컴포넌트만 감지함 (Overlap 아님)
-SkeletalMesh 콜리전이 Physics Asset 바디 위의 최상위 게이트 역할
-WeaponTrace 채널 기본 응답은 Ignore로 설정하고, 명시적으로 Block 설정된 메시 바디만 판정

#루트 모션 주의사항
-ApplyRootMotionMoveToForce는 CMC에 사본을 등록함. 태스크 멤버만 변경해도 CMC 사본에는 반영 안 됨 — CMC의 CurrentRootMotion.RootMotionSources를 직접 순회해서 갱신해야 함

#애니메이션 주의사항
-Two-stage chained Layered Blend Per Bone (UpperBody 적용 → 캐시 → arm 레이어 base로 사용)로 구성하면 GetSlotLocalWeight 동적 가중치 없이 레이어 분리 가능
-양손 무기 레이어 스위칭은 DA에 프로퍼티만 유지 중이며, 구현은 명시적으로 보류 상태

#StateTree 주의사항
-Property Binding은 Context Actor의 서브오브젝트(컴포넌트) 프로퍼티만 노출함. 액터 직속 UPROPERTY 멤버는 노출 안 됨
-태그 기반 Condition은 노드 선택 시점에만 평가됨. 글로벌 전환이 필요하면 Root 레벨 On Event Transition 사용

#응답 원칙
-질문에서 AP 접두사를 사용하면 보통 ActionPractice를 줄여서 부르는 것으로 해석할 것
-직접 구현 요청이 없으면 설계 의도, 시스템 관계, 리팩터링 포인트, 클래스 책임 분리를 우선 설명할 것
-새 코드를 제안할 때는 현재 프로젝트의 GAS, StateTree, InputBuffer, GameplayTagsSubsystem 중심 구조를 해치지 않는 방향으로 제안할 것
-언리얼 5.7에서 유효하지 않은 API나 구버전 방식은 사용하지 말 것
