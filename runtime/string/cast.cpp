#include "hrd_runtime.h"
#include "hrd_string_templete.h"

extern "C" void hrd_cast_s8_to_s16(HrdString16 *out, const HrdString8 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  std::vector<uint32_t> codepoints;
  hrd_decode_utf8(src->data, src->len, codepoints);
  hrd_encode_utf16(out, codepoints);
}

extern "C" void hrd_cast_s8_to_s32(HrdString32 *out, const HrdString8 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  std::vector<uint32_t> codepoints;
  hrd_decode_utf8(src->data, src->len, codepoints);
  hrd_encode_utf32(out, codepoints);
}

extern "C" void hrd_cast_s16_to_s8(HrdString8 *out, const HrdString16 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  std::vector<uint32_t> codepoints;
  hrd_decode_utf16(src->data, src->len, codepoints);
  hrd_encode_utf8(out, codepoints);
}

extern "C" void hrd_cast_s16_to_s32(HrdString32 *out, const HrdString16 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  std::vector<uint32_t> codepoints;
  hrd_decode_utf16(src->data, src->len, codepoints);
  hrd_encode_utf32(out, codepoints);
}

extern "C" void hrd_cast_s32_to_s8(HrdString8 *out, const HrdString32 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  std::vector<uint32_t> codepoints;
  hrd_decode_utf32(src->data, src->len, codepoints);
  hrd_encode_utf8(out, codepoints);
}

extern "C" void hrd_cast_s32_to_s16(HrdString16 *out, const HrdString32 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  std::vector<uint32_t> codepoints;
  hrd_decode_utf32(src->data, src->len, codepoints);
  hrd_encode_utf16(out, codepoints);
}