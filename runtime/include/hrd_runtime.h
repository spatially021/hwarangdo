#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
extern "C" {
using HrdHandle = uint64_t;
using HrdDestroyFn = void (*)(void *);
using HrdOnDestroyFn = void (*)(void *);

struct HrdEntitySlot {
  void *ptr = nullptr;
  HrdOnDestroyFn onDestroy = nullptr;
  HrdDestroyFn destroy = nullptr;

  bool alive = false;
  bool destroyPending = false;
  uint32_t generation = 1;
};

struct HrdString8 {
  uint8_t *data;
  uint64_t len;
  uint64_t cap;
};

struct HrdWorld {
  bool running = true;
  std::vector<HrdEntitySlot> entities;
  std::vector<uint32_t> freeSlots;
  std::vector<uint32_t> destroyQueue;
};

inline void hrd_runtime_panic(const char *msg) {
  std::fprintf(stderr, "HRD runtime error: %s\n", msg);
  std::abort();
}

HrdWorld *hrd_world_create();

bool hrd_world_running();
void hrd_world_flush_destroy();

void hrd_world_quit();
void hrd_world_destroy();

extern "C" HrdHandle hrd_world_spawn_raw(void *entity, HrdOnDestroyFn onDestroy,
                                         HrdDestroyFn destroy);
void *hrd_world_view_raw(HrdHandle handle);
void hrd_world_destroy_entity_raw(HrdHandle handle);
}

extern "C" void hrd_log_info_i32(int32_t v);
extern "C" void hrd_log_info_u32(uint32_t v);
extern "C" void hrd_log_info_f32(float v);
extern "C" void hrd_log_info_bool(bool v);
extern "C" void hrd_log_info_c8(char v);
extern "C" void hrd_log_info_s8(HrdString8 *s);

extern "C" bool hrd_string_eq_s8(HrdString8 *a, HrdString8 *b);
extern "C" bool hrd_string_ne_s8(HrdString8 *a, HrdString8 *b);

extern "C" void hrd_s8_from_literal(HrdString8 *out, const uint8_t *data,
                                    uint64_t len);

extern "C" void hrd_copy_s8(HrdString8 *out, HrdString8 *src);

extern "C" void hrd_move_s8(HrdString8 *out, HrdString8 *src);

extern "C" void hrd_destroy_s8(HrdString8 *s);

extern "C" void hrd_add_s8(HrdString8 *out, HrdString8 *a, HrdString8 *b);
