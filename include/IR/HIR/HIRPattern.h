#pragma once

#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRSymbol.h"
#include <utility>
#include <variant>

struct HIRLiteralExpr;

struct HIRPattern : HIRNode {
  explicit HIRPattern(SourceSpan s, HIRNodeKind k) : HIRNode(s, k) {}
  virtual ~HIRPattern() = default;
};

struct HIRLiteralCase {
  unique_ptr<HIRLiteralExpr> expr = nullptr;
  HIRLiteralCase(unique_ptr<HIRLiteralExpr> e) : expr(std::move(e)) {}
};

struct HIRUnitCase {
  HIREnumVariant *variant = nullptr;
  HIRUnitCase(HIREnumVariant *v) : variant(v) {}
};

struct HIRPayloadCase {
  HIREnumVariant *variant = nullptr;
  HIRLocal *binding = nullptr;
  HIRPayloadCase(HIREnumVariant *v, HIRLocal *b) : variant(v), binding(b) {}
};

struct HIRWildcardCase {};

struct HIRCasePattern : HIRPattern {
  using Selector = std::variant<HIRLiteralCase, HIRUnitCase, HIRPayloadCase,
                                HIRWildcardCase>;

  Selector selector;

  explicit HIRCasePattern(SourceSpan s, HIRLiteralCase v)
      : HIRPattern(s, HIRNodeKind::CasePattern), selector(std::move(v)) {}

  explicit HIRCasePattern(SourceSpan s, HIRUnitCase c)
      : HIRPattern(s, HIRNodeKind::CasePattern), selector(c) {}

  explicit HIRCasePattern(SourceSpan s, HIRPayloadCase p)
      : HIRPattern(s, HIRNodeKind::CasePattern), selector(p) {}
  explicit HIRCasePattern(SourceSpan s, HIRWildcardCase w)
      : HIRPattern(s, HIRNodeKind::CasePattern), selector(w) {}
};
