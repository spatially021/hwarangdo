#pragma once

#include "hrd/Token.h"
#include "hrd/enums/AccessModifier.h"
#include <optional>

struct DeclPrefix {
  AModifier modi = AModifier::PUBLIC;
  Token startToken;
  bool isExtern = false;
  bool isConst = false;
  bool isRoot = false;
  bool isFrame = false;
  bool isOverride = false;
  bool isAsync = false;
  bool isStatic = false;

  optional<string> linkName;
};