#pragma once

#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include <utility>
#include <variant>

struct HIRLiteralExpr;

struct HIRPattern : HIRNode {
  explicit HIRPattern(SourceSpan s, HIRNodeKind k) : HIRNode(s, k) {}
  virtual ~HIRPattern() = default;
};

struct HIRLiteralCase {
  std::unique_ptr<HIRLiteralExpr> expr = nullptr;
  HIRLiteralCase(std::unique_ptr<HIRLiteralExpr> e) : expr(std::move(e)) {}
};

struct HIRUnitCase {
  EnumVariantSymbol *variant = nullptr;
  HIRUnitCase(EnumVariantSymbol *v) : variant(v) {}
};

struct HIRPayloadCase {
  EnumVariantSymbol *variant = nullptr;
  HIRLocal *binding = nullptr;
  HIRPayloadCase(EnumVariantSymbol *v, HIRLocal *b) : variant(v), binding(b) {}
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
