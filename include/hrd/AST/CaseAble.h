#pragma once

#include "hrd/AST/ASTNode.h"
#include "hrd/AST/CaseKey.h"
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

class Case;

enum class SwitchKind { Switch, Match };

class CaseAble {
public:
  ExprPtr value; // switch (value)
  std::vector<std::shared_ptr<Case>>
      clauses; // CaseStmt 또는 DefaultStmt 의 집합
  std::unordered_set<CaseKey, CaseKeyHash> caseKeys;
  std::unordered_set<EnumVariantSymbol *> usedVariants;
  bool hasDefault = false;
  SwitchKind sKind;
  CaseAble(ExprPtr v, std::vector<shared_ptr<Case>> c, SwitchKind k)
      : value(std::move(v)), clauses(std::move(c)), sKind(k) {}
};