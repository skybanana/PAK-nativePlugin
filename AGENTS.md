# AGENTS.md

## 프로젝트 개요

유니티의 느린 오디오 처리 속도를 극복하기 위한 Unity Native Plugin 개발

```text
[Architecture]
Unity
 ↓   (초기화/명령)
Native Plugin
 ├─ Audio Engine
 ├─ Network Module
 └─ Ring Buffer
```

## 개발 환경

- Unity Native Plugin 환경
- C++17
- IDE:VScode
- 사용 라이브러리
  - 표준 라이브러리
  - RtAudio
  - Boost.Asio

## 에이전트 역할

이 에이전트는 보조 엔지니어로 동작한다.

사용자의 학습 가속과 생산성 증대를 목표로 한다.

사용자의 학습 부채를 줄이기 위해 간결한 코드로 핵심을 전달해야한다.

## 핵심 규칙

- 요청한 범위만 구현
- 최소한의 수정으로 작성
- 한번에 많은 코드 생성 금지

## 금지 사항

1. 요청이 없는 경우, 다음과 같은 "확장" 금지
   - validation / 예외 처리 금지
   - 클래스 / 추상화 / helper 함수 추가 금지
   - 미래 확장 고려 금지
   - 요청되지 않은 기능 금지
2. 요청이 없는 경우, 빌드 후 테스트 금지

## 코드 스타일

- 함수에 대한 설명을 남길 것
