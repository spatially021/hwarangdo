// native/test.c

#include <stdint.h>

int32_t hrd_test_add(int32_t a, int32_t b) { return a + b; }

int32_t hrd_test_mul(int32_t a, int32_t b) { return a * b; }

void hrd_test_print_value(int32_t value) {
  // printf까지 끌고 오기 싫으면 일단 비워둬도 됨.
  // 반환 없는 extern 호출 테스트용.
  (void)value;
}