#include "hrd_runtime.h"

#include <cinttypes>
#include <cstdio>

extern "C" void hrd_log_info_s8(HrdString8 *s) {
  if (s == nullptr || s->data == nullptr) {
    std::fputc('\n', stdout);
    return;
  }

  std::fwrite(s->data, 1, s->len, stdout);
  std::fputc('\n', stdout);
}
extern "C" void hrd_log_info_i32(int32_t v) { std::printf("%" PRId32 "\n", v); }
extern "C" void hrd_log_info_u32(uint32_t v) {
  std::printf("%" PRIu32 "\n", v);
}
extern "C" void hrd_log_info_f32(float v) {
  std::printf("%g\n", static_cast<double>(v));
}
extern "C" void hrd_log_info_bool(bool v) { std::puts(v ? "true" : "false"); }
extern "C" void hrd_log_info_c8(char v) { std::printf("%c\n", v); }