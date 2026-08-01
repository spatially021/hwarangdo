#include "hrd_runtime.h"
#include "hrd_string_templete.h"

extern "C" bool hrd_string_eq_s8(const HrdString8 *a, const HrdString8 *b) {
  return hrd_string_eq_impl<HrdString8, uint8_t>(a, b);
}

extern "C" bool hrd_string_ne_s8(const HrdString8 *a, const HrdString8 *b) {
  return !hrd_string_eq_s8(a, b);
}

extern "C" void hrd_s8_from_literal(HrdString8 *out, const uint8_t *data,
                                    uint64_t len) {
  hrd_from_literal_impl<HrdString8, uint8_t>(out, data, len);
}

extern "C" void hrd_copy_s8(HrdString8 *out, const HrdString8 *src) {
  hrd_copy_string_impl<HrdString8, uint8_t>(out, src);
}

extern "C" void hrd_move_s8(HrdString8 *out, HrdString8 *src) {
  hrd_move_string_impl(out, src);
}

extern "C" void hrd_destroy_s8(HrdString8 *string) {
  hrd_destroy_string_impl(string);
}

extern "C" void hrd_add_s8(HrdString8 *out, const HrdString8 *a,
                           const HrdString8 *b) {
  hrd_add_string_impl<HrdString8, uint8_t>(out, a, b);
}