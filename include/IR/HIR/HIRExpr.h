#pragma once

#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "enums/Operator.h"
#include "vector"
#include <memory>
#include <utility>

struct HIRMethodDecl;

using std::unique_ptr;

enum class HIRExprCategory {
  Value,
  Place,
};

struct HIRExpr : HIRNode {
  HIRType *type = nullptr;
  HIRExprCategory category = HIRExprCategory::Value;

  HIRExpr(HIRNodeKind k, HIRType *ty, HIRExprCategory c, SourceSpan s = {})
      : HIRNode(k, s), type(ty), category(c) {}
  virtual ~HIRExpr() = default;
};

struct HIRPlaceExpr : HIRExpr {
  HIRPlaceExpr(HIRNodeKind k, HIRType *ty, SourceSpan s = {})
      : HIRExpr(k, ty, HIRExprCategory::Place, s) {}
};

struct HIRValueExpr : HIRExpr {
  HIRValueExpr(HIRNodeKind k, HIRType *ty, SourceSpan s = {})
      : HIRExpr(k, ty, HIRExprCategory::Value, s) {}
};

struct HIRLocalPlaceExpr : HIRPlaceExpr {
  HIRLocal *local = nullptr;

  HIRLocalPlaceExpr(HIRLocal *l, SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::LocalPlaceExpr, l->type, s), local(l) {}
};

struct HIRParamPlaceExpr : HIRPlaceExpr {
  HIRParam *param = nullptr;

  HIRParamPlaceExpr(HIRParam *p, SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::ParamPlaceExpr, p->type, s), param(p) {}
};

struct HIRThisPlaceExpr : HIRPlaceExpr {
  HIRThisPlaceExpr(HIROserverType *ty, SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::ThisPlaceExpr, ty, s) {}
};

struct HIRSuperPlaceExpr : HIRPlaceExpr {
  HIRSuperPlaceExpr(HIROserverType *ty, SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::SuperPlaceExpr, ty, s) {}
};

enum class HIRFieldAccessMode {
  ValueObject,
  ObserverObject,
};

struct HIRFieldPlaceExpr : HIRPlaceExpr {
  HIRTypeDecl *object = nullptr; // value or observer
  HIRField *field = nullptr;
  HIRFieldAccessMode accessMode = HIRFieldAccessMode::ValueObject;

  HIRFieldPlaceExpr(HIRTypeDecl *obj, HIRField *f, HIRFieldAccessMode mode,
                    SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::FieldPlaceExpr, f->type, s), object(obj),
        field(f), accessMode(mode) {}
};

struct HIRLiteralExpr : HIRValueExpr {
  ResolvedLit resolvedLit = {};

  HIRLiteralExpr(HIRType *ty, ResolvedLit rl, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::LiteralExpr, ty, s), resolvedLit(rl) {}
};

struct HIRLoadExpr : HIRValueExpr {
  HIRPlaceExpr *place = nullptr;

  HIRLoadExpr(HIRPlaceExpr *p, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::LoadExpr, p->type, s), place(p) {}
};

struct HIRAssignExpr : HIRValueExpr {
  HIRPlaceExpr *lhs = nullptr;
  HIRExpr *rhs = nullptr;

  HIRAssignExpr(HIRPlaceExpr *l, HIRExpr *r, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::AssignExpr, l->type, s), lhs(l), rhs(r) {}
};

struct HIRUnaryExpr : HIRValueExpr {
  Operator op;
  unique_ptr<HIRExpr> operand = nullptr;

  HIRUnaryExpr(HIRType *ty, Operator o, unique_ptr<HIRExpr> in,
               SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::UnaryExpr, ty, s), op(o),
        operand(std::move(in)) {}
};

struct HIRBinaryExpr : HIRValueExpr {
  Operator op;
  unique_ptr<HIRExpr> left = nullptr;
  unique_ptr<HIRExpr> right = nullptr;

  HIRBinaryExpr(HIRType *ty, Operator o, unique_ptr<HIRExpr> l,
                unique_ptr<HIRExpr> r, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::BinaryExpr, ty, s), op(o), left(std::move(l)),
        right(std::move(r)) {}
};

struct HIRCastExpr : HIRValueExpr {
  HIRExpr *value = nullptr;
  HIRType *fromType = nullptr;
  HIRType *toType = nullptr;

  HIRCastExpr(HIRExpr *v, HIRType *from, HIRType *to, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::CastExpr, to, s), value(v), fromType(from),
        toType(to) {}
};

struct HIRCallExpr : HIRValueExpr {
  HIRMethodDecl *callee = nullptr;
  std::vector<HIRExpr *> args;

  HIRCallExpr(HIRMethodDecl *m, std::vector<HIRExpr *> a, HIRType *r,
              SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::CallExpr, r, s), callee(m),
        args(std::move(a)) {}
};

enum class HIRReceiverMode {
  Value,
  Observer,
};

struct HIRMethodCallExpr : HIRValueExpr {
  HIRExpr *receiver = nullptr;
  HIRMethodDecl *method = nullptr;
  std::vector<HIRExpr *> args;
  HIRReceiverMode receiverMode = HIRReceiverMode::Value;

  HIRMethodCallExpr(HIRExpr *recv, HIRMethodDecl *m, std::vector<HIRExpr *> a,
                    HIRReceiverMode rm, HIRType *r, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::MethodCallExpr, r, s), receiver(recv),
        method(m), args(std::move(a)), receiverMode(rm) {}
};

struct HIRSpawnExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  HIREntityType *entityType = nullptr;
  HIRMethodDecl *initMethod = nullptr; // 없으면 기본 생성 의미
  std::vector<HIRExpr *> args;

  HIRSpawnExpr(HIRHandleType *outType, StorageKind st, HIREntityType *ent,
               HIRMethodDecl *init, std::vector<HIRExpr *> a, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::SpawnExpr, outType, s), storage(st),
        entityType(ent), initMethod(init), args(std::move(a)) {}
};

struct HIRViewExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  HIRExpr *handle = nullptr;
  HIREntityType *entityType = nullptr;

  HIRViewExpr(HIROserverType *outType, StorageKind st, HIRExpr *h,
              HIREntityType *ent, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::ViewExpr, outType, s), storage(st), handle(h),
        entityType(ent) {}
};

struct HIRPattern : HIRNode {
  explicit HIRPattern(HIRNodeKind k, SourceSpan s = {}) : HIRNode(k, s) {}
  virtual ~HIRPattern() = default;
};

struct HIRLiteralPattern : HIRPattern {
  HIRLiteralExpr *value = nullptr;

  explicit HIRLiteralPattern(HIRLiteralExpr *v, SourceSpan s = {})
      : HIRPattern(HIRNodeKind::LiteralPattern, s), value(v) {}
};

struct HIREnumPattern : HIRPattern {
  HIREnumVariant *variant = nullptr;

  explicit HIREnumPattern(HIREnumVariant *v, SourceSpan s = {})
      : HIRPattern(HIRNodeKind::EnumPattern, s), variant(v) {}
};

struct HIRWildcardPattern : HIRPattern {
  HIRWildcardPattern(SourceSpan s = {})
      : HIRPattern(HIRNodeKind::WildcardPattern, s) {}
};

struct HIRMatchArm {
  HIRPattern *pattern = nullptr;
  struct HIRBlockStmt *body = nullptr;
  HIRExpr *resultValue = nullptr; // << expr; lowered result
};

struct HIRMatchExpr : HIRValueExpr {
  HIRExpr *target = nullptr;
  std::vector<HIRMatchArm> arms;
  bool isExhaustive = false;

  HIRMatchExpr(HIRType *ty, HIRExpr *t, std::vector<HIRMatchArm> a,
               bool exhaustive, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::MatchExpr, ty, s), target(t),
        arms(std::move(a)), isExhaustive(exhaustive) {}
};
