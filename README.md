# 화랑도

화랑도(HwarangDo)는 게임 개발을 목적으로 설계 중인 프로그래밍 언어이다.

이 언어는 객체를 Value와 Entity로 구분하며, Entity를 World를 통해 명시적으로 관리하는 객체 모델을 제공한다.

현재 프로젝트는 컴파일러 프론트엔드 구현 단계에 있으며, LLVM 기반 백엔드를 목표로 개발 중이다.

## 개발 철학

* 게임 런타임에 적합한 객체 수명 관리
* Value와 Entity의 명확한 분리
* World 기반의 명시적 객체 관리
* 암묵적 동작보다 명시적 표현 우선

## 주요 특징

* Entity와 Value를 서로 다른 객체 모델로 관리
* Handle/View 기반 Entity 접근 모델
* Entity의 직접 접근 제한
* 명시적 초기화 규칙
* Trait 기반 인터페이스 시스템
* Match 표현식 및 Enum 지원

## 개발 현황

### 구현 완료

* Lexer
* Parser
* Builder
* Linker
* Resolver
* HIR Builder
* HIR Verifier

### 개발 예정

* MIR Builder
* LIR Builder
* LLVM Backend

## 빌드 방법

### 요구 사항

* CMake
* C++17 이상 지원 컴파일러

### 소스에서 빌드

```bash
cmake --preset release
cmake --build --preset release
```

## 실행

```bash
./build/release/hwarangdo testProject
```

## 문서

* 문법 명세: `docs/grammar.md`
