# 화랑검 (HwarangSword)

## 개요

화랑검은 **게임 엔진과 프로그래밍 언어를 병합**한 차세대 언어입니다.
C/C++ 계열 문법을 기반으로 하되, 포인터와 GC 대신 **Rust의 소유권 개념**을 도입하여 **빠른 속도와 안전한 메모리 관리**를 목표로 합니다.
또한 게임 엔진을 언어 수준에서 통합하여 **언어와 라이브러리 사이의 간극을 최소화**하는 것을 목표로 합니다.

**타깃 사용자:**

* 인디 게임 개발자
* 빠른 프로토타입 제작자
* 성능 최적화가 필요한 게임 개발자

---

## 문법

### 변수 선언

```c++
자료형 변수명;
자료형 변수명 = 값;
```

**예시**

```c++
int a;
int b = 10;
float f = 3.14;
string name = "Hwarang";
boolean flag = true;
```

---

### 자료형

| 자료형     | 크기      | 특징 |
| ------- | ------- | -- |
| int     | 64비트 정수 | -  |
| float   | 32비트 실수 | -  |
| double  | 64비트 실수 | -  |
| char    | 8비트 문자  | -  |
| string  | 문자열     | -  |
| boolean | 논리값     | -  |

**리터럴 지원:**

* 정수: `10`, `0xFF`, `0b1010`
* 실수: `3.14`, `2.0f`
* 문자열: `"hello"`
* 문자: `'A'`
* 불리언: `true`, `false`

---

### 조건문

```c++
if(조건){
    // 코드
} else if(조건){
    // 코드
} else {
    // 코드
}

switch(값){
    case 1:
        // 코드
        break;
    case 2:
        // 코드
        break;
    default:
        // 코드
}
```

---

### 반복문

```c++
for(int i=0; i<10; i++){
    // 코드
}

while(조건){
    // 코드
}

do{
    // 코드
} while(조건);
```

* `break`, `continue` 정상 동작

---

### 함수

* 두 가지 형태 지원:

  * 자료형 기반: `int sum(int a, int b) {}`
  * `func` 키워드 기반: `func sum(int a, int b) {}`
* 반환형 없는 함수는 `void`로 간주
* 반환값이 있으면 해당 자료형으로 변환 가능

**예시**

```c++
func int add(int a, int b){
    return a + b;
}

int result = add(5, 10);
```

* 매개변수는 값 전달(value) 또는 참조(reference) 가능
* 향후 가변인자(varargs), 기본값(default value), 람다 지원 예정

---

### 배열 및 컬렉션

```c++
int[] nums = [1, 2, 3, 4];
string[] names = ["Alice", "Bob"];
map<string, int> scores;
```

* 배열과 동적 배열(vector) 지원
* 맵(dictionary) 지원
* 향후 리스트, 셋 등 컬렉션 확장 예정

---

### 클래스 (미구현)

* 향후 계획:

  * 클래스, 상속, 다형성, 인터페이스/트레이트 지원
  * 엔진 객체와 상호작용 가능한 내장 클래스 설계

---

### 소유권 / 메모리 관리

* 포인터 없음
* **소유권 기반 메모리 관리**:

  * 기본적으로 이동(move) 또는 복사(copy)
  * 참조는 빌림(borrow) 형태로 안전하게 사용
* 예시:

```c++
string a = "Hello";
string b = move(a); // a는 더 이상 사용 불가
```

* GC 없음 → 모든 메모리 수명은 컴파일러가 추적

---

### 엔진 통합 (미구현)

* 렌더링, 물리, 입력, 사운드 등 게임 엔진 기능을 언어 수준에서 통합
* 엔진 API 사용법 예시 제공 예정
* 스크립트와 엔진 간 경계 최소화 목표

---

### 주석

```c++
// 단일 라인 주석
/* 멀티 라인 주석 */
/**
 * 문서용 주석
 */
```

---

## 빌드 및 실행 (예시)

```bash
# 소스 코드 컴파일
hwarangsword build main.hs

# 실행
./main
```

---

### Hello World 예시

```c++
func void main() {
    string name = "HwarangSword";
    print("Hello, " + name);
}
```
