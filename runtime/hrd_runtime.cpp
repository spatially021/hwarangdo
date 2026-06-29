#include "runtimes/hrd_runtime.h"

#include <cstdio>

extern "C" void hrd_log_info_s8(HrdString8 s) {
  std::fwrite(s.data, 1, s.len, stdout);
  std::fputc('\n', stdout);
}