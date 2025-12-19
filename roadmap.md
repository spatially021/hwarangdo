# 🚀 게임 개발용 언어 컴파일러 재작성 로드맵 (체크리스트)

## 1️⃣ 언어 스펙 확정
- [x] **문법(Grammar) 확정**
  - [o] 변수 선언 문법 확정
  - [o] 함수 선언 문법 (명시/묵시) 확정
  - [o] 제어문 문법 확정
  - [o] 블록 및 스코프 관련 문법 확정
  - [o] 모듈/네임스페이스 구조 확정
  - [x] 매니저(엔진 구성 요소) 문법 정의
- [o] **타입 시스템 정의**
  - [o] 기본 타입 집합 확정
  - [o] 함수 타입 정의
  - [o] 배열/슬라이스 타입 확정
  - [o] 구조체 타입 정의
  - [o] 묵시적 반환 타입 추론 규칙 설계
- [o] **소유권(Ownership) 모델 정의**
  - [o] move/copy 규칙
  - [o] borrow 규칙 (& / &mut)
  - [o] mutable/immutable borrow 제한
  - [o] 리소스/매니저 소유권 규칙
- [x] **런타임 모델(Execution Model) 개요 확정**
  - [o] 메모리 관리 전략
  - [x] 매니저 초기화 모델
  - [x] 엔진 구조 설계(ECS 여부 등)

## 2️⃣ 컴파일러 아키텍처 설계
- [o] 전체 파이프라인 설계 (Lexer → Parser → AST → Semantic → IR → Backend)
- [o] 디렉토리 구조 설계
- [x] 공용 컴포넌트 정의
  - [o] Token
  - [x] Span/Location
  - [x] ErrorHandler
  - [x] SymbolTable
  - [x] ScopeManager
  - [x] Type 객체 구조
- [x] 프로젝트 구조 검증 코드 작성

## 3️⃣ AST 재설계
- [o] AST 기본 계층 구조
  - [o] Expr / Stmt / Decl 구조
- [x] 새로운 노드 정의
  - [x] FunctionDecl (explicit/implicit variant)
  - [x] VarDecl
  - [x] StructDecl
  - [x] ArrayDecl / ArrayAccess
  - [x] BorrowExpr / MoveExpr
  - [x] ReturnStmt
- [x] NodeKind 재정리
- [x] Visitor 패턴 기본 틀 생성

## 4️⃣ Parser 재작성
- [ ] 파싱 전략 선택 (Pratt vs Recursive Descent)
- [ ] 기본 파서 루틴
  - [ ] declaration()
  - [ ] functionDecl() (명시/묵시)
  - [ ] varDecl()
  - [ ] statement()
  - [ ] expression()
- [ ] 자료 구조 파싱
  - [ ] 배열
  - [ ] 구조체
- [ ] 소유권/borrow 문법 파싱
- [ ] 오류 복구(panic mode) 구현
- [ ] AST 생성 검증

## 5️⃣ 타입 시스템 구현
- [ ] Type 객체 구현
  - [ ] PrimitiveType
  - [ ] FunctionType
  - [ ] ArrayType
  - [ ] StructType
  - [ ] BorrowedType (&T, &mut T)
- [ ] 타입 규칙 정의
  - [ ] 산술 연산 타입 규칙
  - [ ] 비교 연산 타입 검증
  - [ ] 함수 호출 타입 매칭
  - [ ] 묵시적 반환 타입 추론
  - [ ] 구조체 필드 타입 확인

## 6️⃣ Ownership / Borrow 체크 시스템
- [ ] Ownership state machine 정의
  - [ ] Owned / Moved / Borrowed / MutBorrowed / Released
- [ ] borrow 규칙 구현
  - [ ] mutable borrow 단일성
  - [ ] immutable borrow 복수 허용
  - [ ] borrow 중 move 금지
  - [ ] move-after-borrow 검증
- [ ] 매니저 리소스 전용 소유권 정책 구현

## 7️⃣ SemanticAnalyzer 재작성
- [ ] Semantic Pass 분할 설계
  - [ ] 선언 수집 Pass
  - [ ] 타입 해석 Pass
  - [ ] Ownership/Borrow Pass
  - [ ] Flow control Pass
  - [ ] Return validation Pass
- [ ] 오류 메시지 체계 개선
- [ ] 실제 코드 기반 통합 테스트 진행

## 8️⃣ IR / Backend / Runtime 구축
- [ ] IR 설계 결정 (자체 IR or LLVM IR)
- [ ] IR 생성기 작성
- [ ] IR 최적화 Pass 설계
- [ ] Backend 코드 생성기 작성
- [ ] 런타임 구성
  - [ ] 메모리 할당기
  - [ ] 매니저 초기화 시스템
  - [ ] 기본 ECS or 엔진 코어
