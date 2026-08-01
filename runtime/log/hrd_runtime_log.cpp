#include "hrd_runtime.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "hrd_runtime.h"

#include <cinttypes>
#include <cstdio>
#include <string>

static void appendUtf8(std::string &out, char32_t cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

//-------------------------
// 8bit
//-------------------------

extern "C" void hrd_log_info_s8(HrdString8 *s) {
  if (s == nullptr || s->data == nullptr) {
    std::fputc('\n', stdout);
    return;
  }

  std::fwrite(s->data, sizeof(uint8_t), s->len, stdout);
  std::fputc('\n', stdout);
}

//-------------------------
// 16bit
//-------------------------

extern "C" void hrd_log_info_s16(HrdString16 *s) {
  if (s == nullptr || s->data == nullptr) {
    std::fputc('\n', stdout);
    return;
  }

  std::string utf8;
  utf8.reserve(s->len);

  for (uint64_t i = 0; i < s->len; ++i) {
    char32_t cp = s->data[i];

    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (i + 1 >= s->len) {
        appendUtf8(utf8, 0xFFFD);
        break;
      }

      char32_t low = s->data[++i];

      if (low < 0xDC00 || low > 0xDFFF) {
        appendUtf8(utf8, 0xFFFD);
        --i;
        continue;
      }

      cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
      cp = 0xFFFD;
    }

    appendUtf8(utf8, cp);
  }

  std::fwrite(utf8.data(), 1, utf8.size(), stdout);
  std::fputc('\n', stdout);
}

//-------------------------
// 32bit
//-------------------------

extern "C" void hrd_log_info_i32(int32_t v) { std::printf("%" PRId32 "\n", v); }
extern "C" void hrd_log_info_u32(uint32_t v) {
  std::printf("%" PRIu32 "\n", v);
}
extern "C" void hrd_log_info_f32(float v) {
  std::printf("%g\n", static_cast<double>(v));
}
extern "C" void hrd_log_info_bool(bool v) { std::puts(v ? "true" : "false"); }
extern "C" void hrd_log_info_c8(char v) { std::printf("%c\n", v); }
extern "C" void hrd_log_info_s32(HrdString32 *s) {
  if (s == nullptr || s->data == nullptr) {
    std::fputc('\n', stdout);
    return;
  }

  std::string utf8;
  utf8.reserve(s->len);

  for (uint64_t i = 0; i < s->len; ++i) {
    char32_t cp = s->data[i];

    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
      cp = 0xFFFD;
    }

    appendUtf8(utf8, cp);
  }

  std::fwrite(utf8.data(), 1, utf8.size(), stdout);
  std::fputc('\n', stdout);
}