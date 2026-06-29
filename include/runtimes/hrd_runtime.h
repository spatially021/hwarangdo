#pragma once

#include <cstdint>
extern "C" {

struct HrdString8 {
  const char *data;
  uint64_t len;
};

void hrd_log_info_s8(HrdString8 s);
}