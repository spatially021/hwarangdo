#include "hrd_runtime.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static HrdString8 hrd_empty_s8() {
  return HrdString8{
      nullptr,
      0,
      0,
  };
}

extern "C" bool hrd_string_eq_s8(HrdString8 *a, HrdString8 *b) {
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

  return std::memcmp(a->data, b->data, a->len) == 0;
}

extern "C" bool hrd_string_ne_s8(HrdString8 *a, HrdString8 *b) {
  return !hrd_string_eq_s8(a, b);
}

extern "C" void hrd_s8_from_literal(HrdString8 *out, const uint8_t *data,
                                    uint64_t len) {
  if (out == nullptr) {
    std::abort();
  }

  if (len == 0) {
    *out = hrd_empty_s8();
    return;
  }

  uint8_t *buf = static_cast<uint8_t *>(std::malloc(len));
  if (buf == nullptr) {
    std::abort();
  }

  std::memcpy(buf, data, len);
  *out = HrdString8{buf, len, len};
}

extern "C" void hrd_copy_s8(HrdString8 *out, HrdString8 *src) {
  if (out == nullptr) {
    std::abort();
  }

  if (src->len == 0) {
    *out = hrd_empty_s8();
    return;
  }

  if (src->data == nullptr) {
    std::abort();
  }

  uint8_t *buf = static_cast<uint8_t *>(std::malloc(src->len));
  if (buf == nullptr) {
    std::abort();
  }

  std::memcpy(buf, src->data, src->len);
  *out = HrdString8{buf, src->len, src->len};
}

extern "C" void hrd_move_s8(HrdString8 *out, HrdString8 *src) {
  if (out == nullptr || src == nullptr) {
    std::abort();
  }

  *out = *src;
  *src = hrd_empty_s8();
}

extern "C" void hrd_destroy_s8(HrdString8 *s) {
  if (s == nullptr) {
    std::puts("destroy s8: nullptr");
    return;
  }

  // if (s->data) {
  //   std::printf("destroy s8: \"%.*s\" (len=%llu)\n", (int)s->len, s->data,
  //               (unsigned long long)s->len);
  // } else {
  //   std::printf("destroy s8: <null> (len=%llu)\n", (unsigned long
  //   long)s->len);
  // }

  std::free(s->data);
  *s = hrd_empty_s8();
}
extern "C" void hrd_add_s8(HrdString8 *out, HrdString8 *a, HrdString8 *b) {
  if (out == nullptr) {
    std::abort();
  }

  if (a->len == 0) {
    hrd_copy_s8(out, b);
    return;
  }

  if (b->len == 0) {
    hrd_copy_s8(out, a);
    return;
  }

  if (a->data == nullptr || b->data == nullptr) {
    std::abort();
  }

  if (UINT64_MAX - a->len < b->len) {
    std::abort();
  }

  uint64_t len = a->len + b->len;
  uint8_t *buf = static_cast<uint8_t *>(std::malloc(len));
  if (buf == nullptr) {
    std::abort();
  }

  std::memcpy(buf, a->data, a->len);
  std::memcpy(buf + a->len, b->data, b->len);

  *out = HrdString8{buf, len, len};
}