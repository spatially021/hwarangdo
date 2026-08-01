#include "hrd_runtime.h"
#include "hrd_string_templete.h"

extern "C" bool hrd_string_eq_s32(const HrdString32 *a, const HrdString32 *b) {
  return hrd_string_eq_impl<HrdString32, uint32_t>(a, b);
}

extern "C" bool hrd_string_ne_s32(const HrdString32 *a, const HrdString32 *b) {
  return !hrd_string_eq_s32(a, b);
}

extern "C" void hrd_s32_from_literal(HrdString32 *out, const uint32_t *data,
                                     uint64_t len) {
  hrd_from_literal_impl<HrdString32, uint32_t>(out, data, len);
}

extern "C" void hrd_copy_s32(HrdString32 *out, const HrdString32 *src) {
  hrd_copy_string_impl<HrdString32, uint32_t>(out, src);
}

extern "C" void hrd_move_s32(HrdString32 *out, HrdString32 *src) {
  hrd_move_string_impl(out, src);
}

extern "C" void hrd_destroy_s32(HrdString32 *string) {
  hrd_destroy_string_impl(string);
}

extern "C" void hrd_add_s32(HrdString32 *out, const HrdString32 *a,
                            const HrdString32 *b) {
  hrd_add_string_impl<HrdString32, uint32_t>(out, a, b);
}