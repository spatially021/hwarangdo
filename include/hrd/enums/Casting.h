#pragma once

enum class CastingResultKind {
  None,
  // 값 범위 문제
  Overflow,  // 값 범위 초과
  Underflow, // 음수 overflow / 너무 작은 값 (선택)

  // 부호 문제
  SignToUnsign,       // signed -> unsigned 위험
  NegativeToUnsigned, // 음수를 unsigned로 변환 시도

  // 정밀도 문제
  PrecisionLoss, // float 축소 / int->float 정확도 손실
  FractionLoss,  // float -> int 시 소수부 손실

  // 타입 계열 문제
  Unmatched,       // 완전히 무관한 타입
  InvalidCategory, // numeric <-> string 같은 계열 자체 불가

  // 언어 정책 문제
  ExplicitRequired, // 명시적 cast 필요
  Narrowing,        // 안전하지 않은 축소 변환

  // 특수값
  NaN,
  Infinity,

  // 내부 처리용
  NotImplemented,
};