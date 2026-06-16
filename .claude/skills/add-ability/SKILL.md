---
name: add-ability
description: 새 GAS 어빌리티나 전투 액션을 추가할 때의 부모 클래스 선택, 태스크 구성, 태그 흐름 절차. 새 Ability 클래스를 만들거나 공격/방어/회피/패링 등 전투 액션을 추가·확장할 때 사용한다.
---

#새 어빌리티 추가 절차

새 어빌리티를 추가할 때는 아래 순서로 판단한다. 직접 구현 요청이 없는 경우,
부모 클래스 선택·상속 이유·태스크 구성 방식·태그 흐름까지 먼저 설명한다.

##1단계: 분류
새 어빌리티가 아래 중 무엇인지부터 분류한다.
-단순 즉발형
-홀드형
-몽타주 기반
-리커버리 기반

##2단계: 부모 클래스 선택
-플레이어 어빌리티는 UBaseAbility → UActionPracticeAbility → 세부 어빌리티 계층을 먼저 확인하고, 기존 계층에서 해결 가능한지 우선 검토한다.
-적 어빌리티는 UBaseAbility → UEnemyAbility 계층을 먼저 확인하고, 보스 전투 흐름과 StateTree 연동 여부를 함께 검토한다.

###몽타주 기반인 경우
-몽타주를 사용하는 어빌리티라면 IMontageAbilityInterface 상속이 필요한지 먼저 확인한다.
-IMontageAbilityInterface를 사용하는 경우 아래 흐름을 프로젝트 기존 패턴에 맞춰 구성한다.
  -SetMontageToPlayTask
  -SetUpPlayMontageWithEventsTask
  -StartMontageWithEventsTask
  -OnTaskMontageCompleted
  -OnTaskMontageInterrupted

###리커버리 기반인 경우
-몽타주 재생 중 입력 저장, 조작 제한, 리커버리 상태 태그 관리, 버퍼 입력 실행이 필요한 어빌리티라면 UActionRecoveryAbility 상속이 적합한지 먼저 확인한다.
-UActionRecoveryAbility는 EnableBufferInput, ActionRecovery 커브 폴링과 Recovering 태그 처리, 버퍼 입력 실행 흐름을 이미 담당한다. 유사 기능이 필요하면 중복 구현보다 상속을 우선 검토한다.

##3단계: 태스크 구성
-태스크를 사용할 때는 매번 직접 생성 패턴을 새로 만들기보다, UBaseAbility에 있는 공용 전처리와 헬퍼를 우선 활용한다.
-WaitGameplayEvent 계열 태스크는 가능하면 UBaseAbility의 CreateWaitGameplayEventTask와 STARTWAITEVENTTASK, ENDABILITYTASK 매크로 패턴을 우선 사용한다.

##4단계: ASC 접근 및 태그/이벤트 흐름
-ASC 접근이 필요하면 파생 어빌리티에서 개별 캐스팅을 남발하기보다 BaseASC / APASC 접근 함수를 먼저 활용한다.
-활성화 대상 어빌리티 spec을 찾을 때는 웬만하면 기존 코드 구현처럼 에셋 태그와 GetActivatableGameplayAbilitySpecsByAllMatchingTags를 사용한다.
-어빌리티를 활성화할 때 이벤트 전달이 필요하면 BaseASC의 TryActivateAbilityWithEventData를 사용한다.
-네트워크 예측 태그 처리나 GameplayEvent 전달이 필요하면 UActionPracticeAbilitySystemComponent의 AddTagNetPredicted, RemoveTagsNetPredicted, HandleGameplayEventNetPredicted 흐름을 우선 사용한다.

##참고
-데미지/리액션이 얽히는 어빌리티라면 docs/ARCHITECTURE.md의 데미지 파이프라인·Attack 시스템 섹션을 함께 확인한다.
-GAS 관련 함정(BlockAbilitiesWithTag 게이트 범위, Periodic GE 캐싱, CommitAbility/Cooldown 등)은 CLAUDE.md의 GAS 핵심 주의사항을 따른다.
