#include "runtimes/hrd_runtime.h"

static HrdWorld *world = nullptr;

static uint32_t hrd_handle_index(HrdHandle h) {
  return static_cast<uint32_t>(h & 0xffffffffu);
}

static uint32_t hrd_handle_generation(HrdHandle h) {
  return static_cast<uint32_t>(h >> 32);
}

static HrdHandle hrd_make_handle(uint32_t index, uint32_t generation) {
  return (static_cast<uint64_t>(generation) << 32) | index;
}

extern "C" HrdWorld *hrd_world_create() {
  world = new HrdWorld{};
  return world;
}

extern "C" bool hrd_world_running() {
  return world != nullptr && world->running;
}

extern "C" void hrd_world_quit() {
  if (world == nullptr) {
    return;
  }

  world->running = false;
}

extern "C" void hrd_world_destroy() {
  delete world;
  world = nullptr;
}
extern "C" HrdHandle hrd_world_spawn_raw(void *entity, HrdDestroyFn destroy,
                                         HrdOnDestroyFn onDestroy) {
  if (!world)
    hrd_runtime_panic("null world");
  if (!entity)
    hrd_runtime_panic("spawn null entity");
  if (!destroy)
    hrd_runtime_panic("spawn null destroy");

  uint32_t i;

  if (!world->freeSlots.empty()) {
    i = world->freeSlots.back();
    world->freeSlots.pop_back();
  } else {
    i = static_cast<uint32_t>(world->entities.size());

    HrdEntitySlot slot{};
    slot.generation = 1;
    world->entities.push_back(slot);
  }

  auto &slot = world->entities[i];
  slot.ptr = entity;
  slot.destroy = destroy;
  slot.onDestroy = onDestroy;
  slot.alive = true;
  slot.destroyPending = false;

  return hrd_make_handle(i, slot.generation);
}

extern "C" void *hrd_world_view_raw(HrdHandle handle) {
  uint32_t index = hrd_handle_index(handle);
  uint32_t generation = hrd_handle_generation(handle);

  std::fprintf(stderr, "view handle: index=%u gen=%u\n", index, generation);

  if (index >= world->entities.size()) {
    hrd_runtime_panic("invalid handle index");
  }

  auto &slot = world->entities[index];

  if (!slot.alive || slot.destroyPending || slot.generation != generation) {
    hrd_runtime_panic("view invalid handle");
  }

  return slot.ptr;
}

extern "C" void hrd_world_destroy_entity_raw(HrdHandle handle) {
  uint32_t index = hrd_handle_index(handle);
  uint32_t generation = hrd_handle_generation(handle);
  std::fprintf(stderr, "destroy handle: index=%u gen=%u\n", index, generation);
  auto &slot = world->entities[index];
  if (!slot.alive || slot.generation != generation) {
    hrd_runtime_panic("destroy invalid handle");
  }
  if (slot.destroyPending) {
    hrd_runtime_panic("destroy already pending entity");
  }
  slot.destroyPending = true;
  world->destroyQueue.push_back(hrd_handle_index(handle));
}
extern "C" void hrd_world_flush_destroy() {
  if (world == nullptr)
    return;

  size_t cursor = 0;

  while (cursor < world->destroyQueue.size()) {
    auto index = world->destroyQueue[cursor++];

    auto &slot = world->entities[index];

    if (!slot.destroyPending)
      continue;

    slot.destroyPending = false;
    slot.alive = false;

    if (slot.onDestroy) {
      slot.onDestroy(slot.ptr);
    }

    if (slot.destroy) {
      slot.destroy(slot.ptr);
    }

    std::free(slot.ptr);

    slot.ptr = nullptr;
    slot.destroy = nullptr;
    slot.onDestroy = nullptr;
    ++slot.generation;

    world->freeSlots.push_back(index);
  }

  world->destroyQueue.clear();
}