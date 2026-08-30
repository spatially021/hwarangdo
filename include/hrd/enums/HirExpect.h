#pragma once

#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/util/Error.h"
#include "magic_enum/magic_enum.hpp"

template <typename T> T *expect(HIRNode *node, HIRNodeKind expected) {
  if (node == nullptr) {
    Error::internal("expected " + std::string(magic_enum::enum_name(expected)) +
                    ", but got nullptr");
  }

  if (node->kind != expected) {
    Error::internal("expected " + std::string(magic_enum::enum_name(expected)) +
                    ", but got " +
                    std::string(magic_enum::enum_name(node->kind)));
  }

  auto *casted = dynamic_cast<T *>(node);
  if (casted == nullptr) {
    Error::internal("failed to cast node. expected kind " +
                    std::string(magic_enum::enum_name(expected)));
  }

  return casted;
}