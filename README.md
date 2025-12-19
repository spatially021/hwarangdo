# 게임 개발용 언어 프로젝트 README (개정판)

이 문서는 지금까지의 논의를 기반으로 정리한 **언어 설계 방향**, **OOP 모델**, **소유권 시스템**, **키워드 규칙**, **엔진 통합 철학**을 포함한 최신 버전의 프로젝트 설명서입니다.

---

# 📌 1. 언어의 목표

이 언어는 **게임 엔진을 언어 레벨에 통합한 시스템 프로그래밍 언어**를 지향합니다.

* C++ 수준의 **퍼포먼스**
* Rust 수준의 **메모리 안전성(소유권/대여 시스템)**
* Unity/C# 같은 **고수준 엔티티/컴포넌트 디자인**
* Unreal 같은 **상속 기반 객체 모델** 지원
* 모든 시스템이 **컴파일러 단위에서 최적화 가능**

즉, "엔진과 언어가 하나인 형태"를 최종 비전으로 합니다.

---

# 📌 2. 객체 모델 (OOP 설계)

언어의 객체 모델은 다음과 같은 **혼합 구조**를 따릅니다.

## ✔ 2.1 기본 데이터 모델: `struct + impl` (Rust 스타일)

* **struct는 순수 데이터**입니다.
* 메서드는 `impl` 블록 안에 정의됩니다.
* 상속 없음 → **zero-cost**
* ECS 컴포넌트 또는 수학 타입(Vec3, Transform 등)에 최적화됩니다.

```c++
struct Vec3 {
    float x;
    float y;
    float z;
};

impl Vec3 {
    func length(&self) -> float {
        return sqrt(self.x*self.x + self.y*self.y + self.z*self.z);
    }
}
```

## ✔ 2.2 고수준 객체 모델: `class` (상속 및 RTTI 지원)

* 게임 엔티티, UI 객체 등 고수준 시스템을 위해 **상속을 지원하는 class**를 제공합니다.
* virtual function / override / 동적 디스패치 제공
* C++ 스타일 vtable 구조를 사용합니다.

```c++
class Entity {
    public virtual func update(&mut self, float dt) {}
};

class Player : Entity {
    public override func update(&mut self, float dt) {
        // ...
    }
};
```

## ✔ 2.3 상속 없는 class → 구조체로 자동 최적화

* 상속/virtual이 없으면 `class` 문법은 컴파일러에 의해 `struct + impl` 형태로 자동 변환됩니다.
* Zero-cost class 모델을 제공합니다.

```c++
class Transform {
    float x;
    float y;
}; // → 내부적으로 struct Transform + impl 로 변환
```

## ✔ 2.4 최종 결론

> 🟦 저수준: struct + impl (값 타입)
>
> 🟩 고수준: class (상속/엔티티, 런타임 다형성)
>
> 🔧 `class`는 상속이 없을 경우 `struct`처럼 자동 최적화됩니다.

이 방식은 **현대 게임엔진 구조에 가장 적합한 하이브리드 객체 모델**입니다.

---

# 📌 3. 소유권/라이프타임 모델

Rust의 ownership 시스템을 기반으로 하되, 게임 개발에서 자주 사용되는 패턴에 맞춰 **더 단순하고 실용적인 모델**을 채택합니다.

## ✔ 3.1 기본 대입 규칙

* **기본 자료형(int, float, char, bool 등)**: 단순 대입 시 값 복사
* **객체/구조체/배열/문자열 등**: 단순 대입 시 소유권 이동(move)
* **명시적 복사 필요 시**: `clone()` 또는 `copy()` 메서드 사용

```c++
int a = 10;
int b = a;       // 값 복사, a 여전히 사용 가능

Vec3 v1 = Vec3{1,2,3};
Vec3 v2 = v1;    // move: v1 소유권이 v2로 이동, v1 사용 불가
Vec3 v3 = v2.clone(); // 명시적 복사
```

## ✔ 3.2 최종 키워드 선택 (조합형)

모든 소유권 관련 표기는 **move만 기호(^)**, 나머지는 영문 키워드로 통일합니다.

| 개념          |      사용 표기 | 의미                         |
| ------------- | -------------: | ---------------------------- |
| 소유권 이동   |           `^x` | move semantics (소유권 이전) |
| 불변 대여     |     `borrow x` | 공유(읽기 전용) 참조         |
| 가변 대여     | `borrow mut x` | 배타적(쓰기 가능) 참조       |
| 공유 참조(RC) |      `share x` | 참조 카운트 기반 공유        |
| 약한 참조     |       `weak x` | 약한 참조 (weak)             |

### 예시

```c++
func foo(Vec3 a) {
    let b = ^a;             // move: a의 소유권을 b로 이동
    let c = borrow b;       // 불변 대여
    let d = borrow mut b;   // 가변 대여
}
```

> 주의: `^x` 표기는 표현식 수준에서 소유권을 이전한다는 의미입니다. 함수 파라미터에서도 `^`를 사용해 호출 시 소유권을 넘길 수 있습니다.

## ✔ 3.3 borrow checker

* Rust와 유사한 정적 borrow 검사기를 구현합니다.
* 게임 개발자에게 더 친절한 오류 메시지와 직관적인 규칙을 제공합니다.

## ✔ 3.4 공유/약한 참조

* Engine 자원(Texture, Sound 등)은 `share`로 관리하는 것이 일반적입니다.
* `weak`는 순환 참조를 방지합니다.

---

# 📌 4. 접근 제어 (Java 스타일)

언어의 접근 제어는 Java 스타일로 설계됩니다.

| 접근 수준 | 의미                                         |
| --------- | -------------------------------------------- |
| public    | 모든 모듈/클래스에서 접근 가능               |
| protected | 상속 관계에서만 접근 가능                    |
| private   | 해당 클래스/struct 내부에서만 접근 가능      |
| extern    | dll등의 함수 로드시 사용                     |

### 게임 엔진 특화 적용

* 엔진 코어: `protected`로 노출 제한
* 게임 스크립트/엔티티: `public`으로 자유롭게 접근
* struct + impl: 기본적으로 `private` 멤버, 필요한 경우 `public`으로 노출

---

# 📌 5. 엔진 통합 모델

이 언어는 **언어 레벨에서 엔진 기능을 직접 노출**할 수 있도록 설계됩니다.

* `class`는 엔티티 시스템의 기본 빌딩 블록
* `struct`는 데이터 중심 컴포넌트
* 런타임은 ECS 스타일로 동작하도록 설계 가능
* 언어 문법으로 `update`, `start`, `onCollision` 같은 엔진 훅을 직접 선언 가능

```c++
class Enemy : Entity {
    public override func update(&mut self, float dt) {
        self.position.x += 1;
    }
};
```

---

# 📌 6. 문법 요소 정리 (통일된 문법)

아래 표기는 현재 스펙에서 일관되게 사용되는 문법 형태입니다.

## 타입 및 선언

```c++
int i;                  // 기본 정수 (platform-independent을 위해 크기 지정 확장 가능)
int i = 10;
int i:64 = 217621220;   // 비트폭 명시(예시 문법)

float f = 3.14;
fixed fi:16.32 = 465.798; // 고정 소수점 예시
char c = 'a';
string name = "Hwarang";
bool flag = true;
```

## 함수 문법

이 언어는 **명시적 선언(explicit)** 과 **묵시적 선언(implicit)**, 두 가지 함수 선언 방식을 지원합니다. 묵시적 선언의 경우 **컴파일러가 반환 타입을 추론**하며, 모든 반환 경로가 호환 가능해야 합니다.

### 1) 명시적 선언 (Explicit)

* 반환형을 명시적으로 적는 방식입니다.
* 문법 예시: `func name(params) -> Type { ... }` 또는 타입 선행 문법 `Type name(params) { ... }`(호환 문법)

```c++

// 타입 선행 문법(호환용)
int add2(int a, int b) {
    return a + b;
}
```

### 2) 묵시적 선언 (Implicit / 타입 추론)

* 반환형을 생략하면 컴파일러가 함수 본문을 분석하여 반환 타입을 추론합니다.
* 반환문이 전혀 없으면 반환형은 `void`로 간주됩니다.
* 여러 반환 경로가 있을 경우, 모든 경로의 타입은 단일 타입으로 **통합(unify)** 되어야 하며, 불일치 시 컴파일 오류가 발생합니다.

```c++
// 반환형 생략 — 컴파일러가 -> int 로 추론
func add_auto(int a, int b) {
    return a + b; // int로 추론
}
```

### 메서드(impl / class)에서의 함수 선언

* 메서드도 위 두 방식 모두 지원합니다.
* 메서드의 경우 `self` 수신자 타입(&self, &mut self, self 등)을 시그니처에 포함할 수 있으며, 묵시적 반환형에서도 동일한 추론 규칙이 적용됩니다.

```c++
impl Vec3 {
    // 명시적
    public func scale(&mut self, float s) -> void {
        self.x *= s;
    }

    // 묵시적
    public func length(&self) {
        return sqrt(self.x*self.x + self.y*self.y + self.z*self.z);
    }
}
```

## struct / impl

```c++
struct Transform {
    Vec3 pos;
    Vec3 rot;
};

impl Transform {
    public func translate(&mut self, Vec3 d) {
        self.pos.x += d.x;
    }
}
```

## class / 상속 / trait

```c++
trait Updatable {
    func update(&mut self, float dt);
}

class GameObject {
    public func update(&mut self, float dt) {}
};

class Player : GameObject, implements Updatable {
    public func update(&mut self, float dt) {
        // ...
    }
};
```

## 소유권 예시

```c++
// 기본 자료형: 값 복사
int a = 10;
int b = a; // a는 여전히 사용 가능

// 객체/struct: move 기본
Vec3 v1 = Vec3{1,2,3};
Vec3 v2 = v1; // v1 소유권 이동, v1 사용 불가
Vec3 v3 = v2.clone(); // 명시적 복사

// move 키워드 명시 가능
func takeOwnership(^Texture t) {
    // t의 소유권을 가져감
}
Texture tex = load_texture();
tak
```

## 메모

### 키워드
상수 키워드
const
전역변수
root

### 선언

top-level에서는 class,struct,impl,trait만 선언가능
그외의 변수 및 함수 선언 혹은 제어문 사용등은 불가능.


### 배열 선언

자료형 이름[크기]; 로 고정

크기에 들어오는 값이 리터럴 혹은 컴파일 타임에서의 상수라면 크기를 검사하고 스택 배열로 선언 그렇지 않다면 힙 배열로 선언.

### trait

trait 이름{
int sum(int a,int b);
}

### enum

enum 이름{
    열거형,
    열거형2,
    열거형3(int),
}