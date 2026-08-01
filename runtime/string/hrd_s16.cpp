#include "hrd_runtime.h"
#include "hrd_string_templete.h"

extern "C" bool hrd_string_eq_s16(const HrdString16 *a, const HrdString16 *b) {
  return hrd_string_eq_impl<HrdString16, uint16_t>(a, b);
}

extern "C" bool hrd_string_ne_s16(const HrdString16 *a, const HrdString16 *b) {
  return !hrd_string_eq_s16(a, b);
}

extern "C" void hrd_s16_from_literal(HrdString16 *out, const uint16_t *data,
                                     uint64_t len) {
  hrd_from_literal_impl<HrdString16, uint16_t>(out, data, len);
}

extern "C" void hrd_copy_s16(HrdString16 *out, const HrdString16 *src) {
  hrd_copy_string_impl<HrdString16, uint16_t>(out, src);
}

extern "C" void hrd_move_s16(HrdString16 *out, HrdString16 *src) {
  hrd_move_string_impl(out, src);
}

extern "C" void hrd_destroy_s16(HrdString16 *string) {
  hrd_destroy_string_impl(string);
}

extern "C" void hrd_add_s16(HrdString16 *out, const HrdString16 *a,
                            const HrdString16 *b) {
  hrd_add_string_impl<HrdString16, uint16_t>(out, a, b);
}