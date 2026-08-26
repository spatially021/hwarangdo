#pragma once
enum class ArgMatchKind {
  Exact,        // 타입 완전 일치
  DefaultArg,   // 호출 인자가 '_' 이고 해당 파라미터에 기본값 존재
  ImplicitCast, // 안전한 암묵 형변환 가능
  Invalid       // 매칭 불가
};