#pragma once

#include "hrd_runtime.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

template <typename StringT> static StringT hrd_empty_string() {
  return StringT{
      nullptr,
      0,
      0,
  };
}

template <typename StringT, typename UnitT>
static bool hrd_string_eq_impl(const StringT *a, const StringT *b) {
  if (a == b) {
    return true;
  }

  if (a == nullptr || b == nullptr) {
    return false;
  }

  if (a->len != b->len) {
    return false;
  }

  if (a->len == 0) {
    return true;
  }

  if (a->data == nullptr || b->data == nullptr) {
    return false;
  }

  if (a->len > UINT64_MAX / sizeof(UnitT)) {
    std::abort();
  }

  const uint64_t byteSize = a->len * sizeof(UnitT);

  return std::memcmp(a->data, b->data, byteSize) == 0;
}

template <typename StringT, typename UnitT>
static void hrd_from_literal_impl(StringT *out, const UnitT *data,
                                  uint64_t len) {
  if (out == nullptr) {
    std::abort();
  }

  if (len == 0) {
    *out = hrd_empty_string<StringT>();
    return;
  }

  if (data == nullptr) {
    std::abort();
  }

  if (len > UINT64_MAX / sizeof(UnitT)) {
    std::abort();
  }

  const uint64_t byteSize = len * sizeof(UnitT);

  auto *buf = static_cast<UnitT *>(std::malloc(byteSize));
  if (buf == nullptr) {
    std::abort();
  }

  std::memcpy(buf, data, byteSize);

  *out = StringT{
      buf,
      len,
      len,
  };
}

template <typename StringT, typename UnitT>
static void hrd_copy_string_impl(StringT *out, const StringT *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  if (out == src) {
    return;
  }

  if (src->len == 0) {
    *out = hrd_empty_string<StringT>();
    return;
  }

  if (src->data == nullptr) {
    std::abort();
  }

  if (src->len > UINT64_MAX / sizeof(UnitT)) {
    std::abort();
  }

  const uint64_t byteSize = src->len * sizeof(UnitT);

  auto *buf = static_cast<UnitT *>(std::malloc(byteSize));
  if (buf == nullptr) {
    std::abort();
  }

  std::memcpy(buf, src->data, byteSize);

  *out = StringT{
      buf,
      src->len,
      src->len,
  };
}

template <typename StringT>
static void hrd_move_string_impl(StringT *out, StringT *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  if (out == src) {
    return;
  }

  *out = *src;
  *src = hrd_empty_string<StringT>();
}

template <typename StringT>
static void hrd_destroy_string_impl(StringT *string) {
  if (string == nullptr) {
    return;
  }

  std::free(string->data);
  *string = hrd_empty_string<StringT>();
}

template <typename StringT, typename UnitT>
static void hrd_add_string_impl(StringT *out, const StringT *a,
                                const StringT *b) {
  if (out == nullptr || a == nullptr || b == nullptr) {
    std::abort();
  }

  if (a->len == 0) {
    hrd_copy_string_impl<StringT, UnitT>(out, b);
    return;
  }

  if (b->len == 0) {
    hrd_copy_string_impl<StringT, UnitT>(out, a);
    return;
  }

  if (a->data == nullptr || b->data == nullptr) {
    std::abort();
  }

  if (UINT64_MAX - a->len < b->len) {
    std::abort();
  }

  const uint64_t len = a->len + b->len;

  if (len > UINT64_MAX / sizeof(UnitT)) {
    std::abort();
  }

  const uint64_t byteSize = len * sizeof(UnitT);
  const uint64_t aByteSize = a->len * sizeof(UnitT);
  const uint64_t bByteSize = b->len * sizeof(UnitT);

  auto *buf = static_cast<UnitT *>(std::malloc(byteSize));
  if (buf == nullptr) {
    std::abort();
  }

  std::memcpy(buf, a->data, aByteSize);
  std::memcpy(buf + a->len, b->data, bByteSize);

  *out = StringT{
      buf,
      len,
      len,
  };
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

static bool hrd_is_valid_codepoint(uint32_t cp) {
  return cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

// ------------------------------------------------------------
// UTF decode
// ------------------------------------------------------------

static void hrd_decode_utf8(const uint8_t *data, uint64_t len,
                            std::vector<uint32_t> &out) {
  if (len != 0 && data == nullptr) {
    std::abort();
  }

  uint64_t i = 0;

  while (i < len) {
    const uint8_t c0 = data[i++];

    if (c0 <= 0x7F) {
      out.push_back(c0);
      continue;
    }

    uint32_t cp = 0;
    uint32_t extra = 0;

    if ((c0 & 0xE0) == 0xC0) {
      cp = c0 & 0x1F;
      extra = 1;

      // overlong: 0xC0, 0xC1
      if (c0 < 0xC2) {
        std::abort();
      }
    } else if ((c0 & 0xF0) == 0xE0) {
      cp = c0 & 0x0F;
      extra = 2;
    } else if ((c0 & 0xF8) == 0xF0) {
      cp = c0 & 0x07;
      extra = 3;

      // UTF-8 only goes to U+10FFFF
      if (c0 > 0xF4) {
        std::abort();
      }
    } else {
      std::abort();
    }

    if (len - i < extra) {
      std::abort();
    }

    for (uint32_t j = 0; j < extra; ++j) {
      const uint8_t cx = data[i++];

      if ((cx & 0xC0) != 0x80) {
        std::abort();
      }

      cp = (cp << 6) | (cx & 0x3F);
    }

    // reject overlong encoding
    if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
        (extra == 3 && cp < 0x10000)) {
      std::abort();
    }

    if (!hrd_is_valid_codepoint(cp)) {
      std::abort();
    }

    out.push_back(cp);
  }
}

static void hrd_decode_utf16(const uint16_t *data, uint64_t len,
                             std::vector<uint32_t> &out) {
  if (len != 0 && data == nullptr) {
    std::abort();
  }

  uint64_t i = 0;

  while (i < len) {
    const uint16_t first = data[i++];

    if (first >= 0xD800 && first <= 0xDBFF) {
      if (i >= len) {
        std::abort();
      }

      const uint16_t second = data[i++];

      if (second < 0xDC00 || second > 0xDFFF) {
        std::abort();
      }

      const uint32_t high = static_cast<uint32_t>(first - 0xD800);
      const uint32_t low = static_cast<uint32_t>(second - 0xDC00);

      const uint32_t cp = 0x10000 + ((high << 10) | low);

      if (!hrd_is_valid_codepoint(cp)) {
        std::abort();
      }

      out.push_back(cp);
      continue;
    }

    // lone low surrogate
    if (first >= 0xDC00 && first <= 0xDFFF) {
      std::abort();
    }

    out.push_back(first);
  }
}

static void hrd_decode_utf32(const uint32_t *data, uint64_t len,
                             std::vector<uint32_t> &out) {
  if (len != 0 && data == nullptr) {
    std::abort();
  }

  if (len > static_cast<uint64_t>(SIZE_MAX)) {
    std::abort();
  }

  out.reserve(static_cast<size_t>(len));

  for (uint64_t i = 0; i < len; ++i) {
    const uint32_t cp = data[i];

    if (!hrd_is_valid_codepoint(cp)) {
      std::abort();
    }

    out.push_back(cp);
  }
}

// ------------------------------------------------------------
// UTF encode
// ------------------------------------------------------------

static uint64_t hrd_utf8_length(const std::vector<uint32_t> &codepoints) {
  uint64_t len = 0;

  for (uint32_t cp : codepoints) {
    uint64_t add = 0;

    if (cp <= 0x7F) {
      add = 1;
    } else if (cp <= 0x7FF) {
      add = 2;
    } else if (cp <= 0xFFFF) {
      add = 3;
    } else {
      add = 4;
    }

    if (UINT64_MAX - len < add) {
      std::abort();
    }

    len += add;
  }

  return len;
}

static uint64_t hrd_utf16_length(const std::vector<uint32_t> &codepoints) {
  uint64_t len = 0;

  for (uint32_t cp : codepoints) {
    const uint64_t add = cp <= 0xFFFF ? 1 : 2;

    if (UINT64_MAX - len < add) {
      std::abort();
    }

    len += add;
  }

  return len;
}

static void hrd_encode_utf8(HrdString8 *out,
                            const std::vector<uint32_t> &codepoints) {
  if (out == nullptr) {
    std::abort();
  }

  const uint64_t len = hrd_utf8_length(codepoints);

  if (len == 0) {
    *out = hrd_empty_string<HrdString8>();
    return;
  }

  auto *buf = static_cast<uint8_t *>(std::malloc(len));

  if (buf == nullptr) {
    std::abort();
  }

  uint64_t i = 0;

  for (uint32_t cp : codepoints) {
    if (!hrd_is_valid_codepoint(cp)) {
      std::abort();
    }

    if (cp <= 0x7F) {
      buf[i++] = static_cast<uint8_t>(cp);
    } else if (cp <= 0x7FF) {
      buf[i++] = static_cast<uint8_t>(0xC0 | (cp >> 6));
      buf[i++] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
      buf[i++] = static_cast<uint8_t>(0xE0 | (cp >> 12));
      buf[i++] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
      buf[i++] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    } else {
      buf[i++] = static_cast<uint8_t>(0xF0 | (cp >> 18));
      buf[i++] = static_cast<uint8_t>(0x80 | ((cp >> 12) & 0x3F));
      buf[i++] = static_cast<uint8_t>(0x80 | ((cp >> 6) & 0x3F));
      buf[i++] = static_cast<uint8_t>(0x80 | (cp & 0x3F));
    }
  }

  *out = HrdString8{
      buf,
      len,
      len,
  };
}

static void hrd_encode_utf16(HrdString16 *out,
                             const std::vector<uint32_t> &codepoints) {
  if (out == nullptr) {
    std::abort();
  }

  const uint64_t len = hrd_utf16_length(codepoints);

  if (len == 0) {
    *out = hrd_empty_string<HrdString16>();
    return;
  }

  if (len > UINT64_MAX / sizeof(uint16_t)) {
    std::abort();
  }

  auto *buf = static_cast<uint16_t *>(std::malloc(len * sizeof(uint16_t)));

  if (buf == nullptr) {
    std::abort();
  }

  uint64_t i = 0;

  for (uint32_t cp : codepoints) {
    if (!hrd_is_valid_codepoint(cp)) {
      std::abort();
    }

    if (cp <= 0xFFFF) {
      buf[i++] = static_cast<uint16_t>(cp);
      continue;
    }

    cp -= 0x10000;

    buf[i++] = static_cast<uint16_t>(0xD800 | ((cp >> 10) & 0x3FF));

    buf[i++] = static_cast<uint16_t>(0xDC00 | (cp & 0x3FF));
  }

  *out = HrdString16{
      buf,
      len,
      len,
  };
}

static void hrd_encode_utf32(HrdString32 *out,
                             const std::vector<uint32_t> &codepoints) {
  if (out == nullptr) {
    std::abort();
  }

  if (codepoints.empty()) {
    *out = hrd_empty_string<HrdString32>();
    return;
  }

  if (codepoints.size() > UINT64_MAX / sizeof(uint32_t)) {
    std::abort();
  }

  const uint64_t len = static_cast<uint64_t>(codepoints.size());

  auto *buf = static_cast<uint32_t *>(std::malloc(len * sizeof(uint32_t)));

  if (buf == nullptr) {
    std::abort();
  }

  for (uint64_t i = 0; i < len; ++i) {
    if (!hrd_is_valid_codepoint(codepoints[i])) {
      std::free(buf);
      std::abort();
    }

    buf[i] = codepoints[i];
  }

  *out = HrdString32{
      buf,
      len,
      len,
  };
}

// static HrdString8 hrd_empty_s8() {
//   return HrdString8{
//       nullptr,
//       0,
//       0,
//   };
// }

// extern "C" bool hrd_string_eq_s8(HrdString8 *a, HrdString8 *b) {
//   if (a == b) {
//     return true;
//   }

//   if (a == nullptr || b == nullptr) {
//     return false;
//   }

//   if (a->len != b->len) {
//     return false;
//   }

//   if (a->len == 0) {
//     return true;
//   }

//   if (a->data == nullptr || b->data == nullptr) {
//     return false;
//   }

//   return std::memcmp(a->data, b->data, a->len) == 0;
// }

// extern "C" bool hrd_string_ne_s8(HrdString8 *a, HrdString8 *b) {
//   return !hrd_string_eq_s8(a, b);
// }

// extern "C" void hrd_s8_from_literal(HrdString8 *out, const uint8_t *data,
//                                     uint64_t len) {
//   if (out == nullptr) {
//     std::abort();
//   }

//   if (len == 0) {
//     *out = hrd_empty_s8();
//     return;
//   }

//   uint8_t *buf = static_cast<uint8_t *>(std::malloc(len));
//   if (buf == nullptr) {
//     std::abort();
//   }

//   std::memcpy(buf, data, len);
//   *out = HrdString8{buf, len, len};
// }

// extern "C" void hrd_copy_s8(HrdString8 *out, HrdString8 *src) {
//   if (out == nullptr) {
//     std::abort();
//   }

//   if (src->len == 0) {
//     *out = hrd_empty_s8();
//     return;
//   }

//   if (src->data == nullptr) {
//     std::abort();
//   }

//   uint8_t *buf = static_cast<uint8_t *>(std::malloc(src->len));
//   if (buf == nullptr) {
//     std::abort();
//   }

//   std::memcpy(buf, src->data, src->len);
//   *out = HrdString8{buf, src->len, src->len};
// }

// extern "C" void hrd_move_s8(HrdString8 *out, HrdString8 *src) {
//   if (out == nullptr || src == nullptr) {
//     std::abort();
//   }

//   *out = *src;
//   *src = hrd_empty_s8();
// }

// extern "C" void hrd_destroy_s8(HrdString8 *s) {
//   if (s == nullptr) {
//     std::puts("destroy s8: nullptr");
//     return;
//   }

//   // if (s->data) {
//   //   std::printf("destroy s8: \"%.*s\" (len=%llu)\n", (int)s->len, s->data,
//   //               (unsigned long long)s->len);
//   // } else {
//   //   std::printf("destroy s8: <null> (len=%llu)\n", (unsigned long
//   //   long)s->len);
//   // }

//   std::free(s->data);
//   *s = hrd_empty_s8();
// }
// extern "C" void hrd_add_s8(HrdString8 *out, HrdString8 *a, HrdString8 *b) {
//   if (out == nullptr) {
//     std::abort();
//   }

//   if (a->len == 0) {
//     hrd_copy_s8(out, b);
//     return;
//   }

//   if (b->len == 0) {
//     hrd_copy_s8(out, a);
//     return;
//   }

//   if (a->data == nullptr || b->data == nullptr) {
//     std::abort();
//   }

//   if (UINT64_MAX - a->len < b->len) {
//     std::abort();
//   }

//   uint64_t len = a->len + b->len;
//   uint8_t *buf = static_cast<uint8_t *>(std::malloc(len));
//   if (buf == nullptr) {
//     std::abort();
//   }

//   std::memcpy(buf, a->data, a->len);
//   std::memcpy(buf + a->len, b->data, b->len);

//   *out = HrdString8{buf, len, len};
// }