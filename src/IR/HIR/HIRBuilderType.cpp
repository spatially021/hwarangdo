#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRType.h"
#include "enums/StorageKind.h"
#include "util/Error.h"
#include <cassert>
#include <memory>
#include <utility>

HIRObserverType *HIRBuilder::getOrCreateObserverType(HIREntityType *entity,
                                                     StorageKind kind) {
  auto it = program->observerCache.find(entity);
  HIRObserverType *result = nullptr;
  if (it == program->observerCache.end()) {
    unique_ptr<HIRObserverType> observer =
        make_unique<HIRObserverType>(entity, kind);
    result = observer.get();
    program->observerCache.emplace(entity, result);
    source->observers.push_back(std::move(observer));
  } else {
    result = it->second;
  }

  if (result == nullptr) {
    Error::internal("fail to get or create observer");
  }

  return result;
}