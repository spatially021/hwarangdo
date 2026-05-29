# 기본 문법 / 선언
- [x] 빈 class / struct / enum / trait 선언
- [x]  접근 제한자 public/private/protected
- [x]  중복 선언 오류
- [x]  타입명과 변수명 충돌 허용 여부
- [x]  함수명과 변수명 충돌 허용 여부
- [x]  잘못된 세미콜론 / 중괄호 오류

# 기본 타입 / 리터럴
- [x] int, float, bool, char, string
- [x] int:i8 ~ i128, u8 ~ u128
- [x] float:f16 ~ f128
- [x] 리터럴 기본 타입 추론
- [x] 리터럴 범위 확장
- [x] 범위 초과 오류
- [ ] signed/unsigned 변환 경고 -> 경고 구현 안됨
- [x] 안전한 암묵 변환
- [x] 불가능한 암묵 변환 오류
- [x] 명시적 형변환

# 변수 / 초기화 / 미초기화
- [x] 선언만 한 value 변수
- [x] 초기화 후 사용
- [x] 초기화 전 읽기 오류
- [x] if 한쪽 분기에서만 초기화 후 사용 오류
- [x] if/else 양쪽 초기화 후 사용 허용
- [x] while 내부 초기화 후 외부 사용 오류
- [x] const 대입 오류
- [x] local / param / field 각각 초기화 검증 

# 연산식
- [x] 산술 연산 + - * / % **
- [x] 비교 연산 == != < <= > >=
- [x] 논리 연산 && || !
- [x] 비트 연산 & | ^ << >>
- [x] 단항 -, !
- [x] unsigned에 단항 - 오류
- [x] bool에 산술 연산 오류
- [x] int에 논리 연산 오류
- [x] 이항 연산에서 양쪽 operand 초기화 확인
- [x] 삼항식 타입 일치 / 공통 타입 변환
- [x] 삼항식 조건 bool 검증

# 대입 / 복합 대입
- [x] 일반 대입 =
- [x] 복합 대입 += -= *= /= %= **=
- [x] 비트 복합 대입 &= |= ^= <<= >>=
- [x] lhs가 place인지 검증
- [x] literal/call/binary를 lhs로 둔 오류
- [x] array element 대입
- [x] field 대입
- [x] const lhs 오류
- [x] 관찰자 재대입 오류
- [x] handle 재대입 정책 확인

# 함수 / 메서드 / 호출
- [x] 명시 반환 타입 함수
- [x] void 함수
- [x] func 반환 타입 추론
- [x] 여러 return 타입 일치
- [x] return 누락 오류
- [x] return; / return expr;
- [x] 인자 개수 오류
- [x] 인자 타입 오류
- [x] 오버로딩 선택
- [x] 모호한 오버로딩 오류
- [x] 기본 인자
- [x] _ 기본값 사용
- [x] _ 사용 불가 케이스
- [x] frame 함수
- [ ] frame 내부 async 호출 금지 -> async 함수 구현 안됨

# struct / init / impl
- [x] struct 선언
- [x] field 선언
- [x] T() 기본 초기화
- [x] T(args) init 호출
- [x] init 없음 + T() 허용
- [x] init 있음 + T() 금지
- [x] init 오버로딩
- [x] init 직접 호출 오류
- [x] init 반환 타입 명시 오류
- [x] init에서 field 초기화 검증
- [x] impl 메서드 호출
- [x] impl 내부 self
- [x] impl 내부 암묵적 field 접근
- [x] local이 field 이름을 가리는 케이스
- [x] class에 impl 금지
- [x] trait 대상 impl

# class / entity / handle / observer
- [x] class 직접 생성 금지
- [x] world.spawn T(...)
- [x] spawn 결과 Handle<T>
- [x] spawn 대상이 struct일 때 오류
- [x] world.view(handle)
- [x] view 결과 관찰자 T
- [x] 관찰자 field 접근
- [x] 관찰자 메서드 호출
- [x] handle 직접 field 접근 오류
- [x] 관찰자 메서드 외부 선언 오류
- [x] 관찰자 선언 시 view 초기화 강제
- [x] 관찰자 복사/대입 오류
- [x] world.destroy(handle)
- [x] destroy 결과값 없음
- [x] destroy를 식으로 사용한 오류

# 제어문
- [x] if 단일문 body
- [x] if block body
- [x] if 조건 bool 검증
- [x] while 조건 bool 검증
- [x] while 내부 break
- [x] while 내부 continue
- [x] loop 밖 break 오류
- [x] loop 밖 continue 오류
- [x] 중첩 loop에서 break/continue
- [x] for range start..end
- [x] for range by
- [ ] for 조건/범위 타입 오류 -> 타입에 따른 기본 by 값 미정으로 인한 검증 불가

# switch / match / enum
- [x] enum unit variant
- [x] enum payload variant
- [x] enum variant 중복 오류
- [x] enum payload에 entity 타입 금지
- [x] variant 호출
- [x] switch case literal
- [x] switch case enum variant
- [x] switch default 위치 마지막 강제
- [x] switch 중복 case 오류
- [x] switch 일부 variant 누락시 default 강제
- [x] switch fallthrough 없음
- [x] switch 내부 break/continue 금지
- [x] match expression
- [x] match << expr;
- [x] match wildcard _
- [x] match _ 위치 마지막 강제
- [x] match default 금지
- [x] match 다중 selector 금지
- [x] match 중복 case 오류
- [x] match 결과 타입 통일
- [x] match exhaustiveness 검사

# 배열
- [x] 고정 배열 선언
- [x] 배열 크기 int literal
- [x] 음수 크기 오류
- [x] 비정수 크기 오류
- [x] 배열 접근
- [x] index int 검증
- [x] array access read
- [x] array access write
- [x] array element 초기화 검증
- [x] array access의 결과 타입이 element type인지 확인

# 상속 / 접근 / override
- [x] class extends
- [x] super 접근
- [x] this 접근
- [x] this를 class 메서드 외부에서 사용 오류
- [x] super를 상속 없는 class에서 사용 오류
- [x] private 접근 제한
- [x] protected 접근 제한
- [x] override 성공
- [x] override 누락 오류
- [x] 존재하지 않는 메서드 override 오류

# trait
- [x] trait 선언
- [x] trait 내부 field 금지
- [x] trait 내부 구현 body 금지
- [x] struct가 impl로 trait 구현
- [x] class가 선언부에서 trait 구현
- [x] trait 중복 구현 오류
- [x] trait impl 내부 일반 메서드 금지
- [x] 계약 메서드 누락 오류

# 에러 회복 / 파서 안정성
- [x] 선언 위치에 호출문이 온 경우
- [x] init(1); 같은 직접 호출 오류
- [x] 괄호 누락
- [x] 중괄호 누락
- [x] 세미콜론 누락
- [x] 잘못된 modifier 조합
- [x] 잘못된 타입 위치 표현식
- [x] 잘못된 식 위치 타입명
- [x] parser가 죽지 않고 diagnostic 출력하는지
