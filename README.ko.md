# 화랑도 (가칭)

handle-view 메모리 모델을 사용하는 게임 개발 특화 언어  
entity는 참조가 아니라 world에 의해 관리된다

English README: README.md

---

## 1. 개요

화랑도는 게임 개발을 위해 설계된 실험적인 프로그래밍 언어이다.

이 언어는 일반적인 참조 기반 객체 모델 대신, handle-view 모델을 사용한다.

- entity는 world에 의해 생성 및 관리된다
- 접근은 view를 통해서만 가능하다
- 수명은 명시적으로 제어된다

이를 통해 예측 가능한 동작, 명확한 수명 관리, 런타임 안전성을 목표로 한다.

---

## 2. 핵심 개념

### Entity vs Value

- class → entity (world 관리 객체)
- struct → value (일반 값)

---

### Handle / View
```
Handle<Player> h = world.spawn Player();

Player p = world.view(h);
p.move();

world.destroy(h);
```
- Handle<T>는 entity를 가리키는 식별자이다
- view는 임시 관찰자를 생성한다
- handle로 직접 접근하는 것은 금지된다

---

### 명시적 수명 관리

- entity는 world.spawn으로 생성된다
- world.destroy로 명시적으로 제거해야 한다
- 제거된 entity 접근은 오류이다

---

## 3. 예제
```
struct Vector {
    int x;
    int y;
}

impl Vector {
    void add(int dx = 0, int dy = 0) {
        x += dx;
        y += dy;
    }
}

class Player {
    Vector pos;

    void move() {
        pos.add(1, _);
    }
}

class Main {
    frame void update() {
        Handle<Player> h = world.spawn Player();

        Player p = world.view(h);
        p.move();

        world.destroy(h);
    }
}
```
---

## 4. 현재 상태

- Lexer / Parser / 의미 분석: 구현됨
- HIR: 진행 중
- Verifier: 진행 중
- 실행 환경: 미구현

아직 설계가 고정되지 않은 초기 단계이다.

---

## 5. 왜 만들었는가

기존 언어들은 범용성을 중심으로 설계되어 있다.

하지만 게임은 다음과 같은 특징을 가진다:

- 프레임 기반 실행
- 명시적인 수명 관리
- 예측 가능한 동작 필요
- world 상태 중심 구조

화랑도는 이러한 구조를 언어 수준에서 표현하려는 시도이다.

---

## 6. 로드맵

- HIR 완성
- Verifier 구현
- MIR 설계
- 실행 모델 구축

---

## 7. 기타

개인 프로젝트이며,  
새로운 설계 방향을 탐구하는 것이 목적이다.