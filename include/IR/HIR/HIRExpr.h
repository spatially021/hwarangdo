#pragma once

#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "vector"
#include <memory>
#include <utility>

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
  HIRExpr *object = nullptr; // value or observer
  HIRField *field = nullptr;
  HIRFieldAccessMode accessMode = HIRFieldAccessMode::ValueObject;

  HIRFieldPlaceExpr(HIRExpr *obj, HIRField *f, HIRFieldAccessMode mode,
                    SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::FieldPlaceExpr, f->type, s), object(obj),
        field(f), accessMode(mode) {}
};

struct HIRTempPlaceExpr : HIRPlaceExpr {
  HIRLocal *temp = nullptr;

  HIRTempPlaceExpr(HIRLocal *t, SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::TempPlaceExpr, t->type, s), temp(t) {}
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

enum class HIRUnaryOp {
  Plus,
  Minus,
  Not,
  BitNot,
};

struct HIRUnaryExpr : HIRValueExpr {
  HIRUnaryOp op;
  HIRExpr *operand = nullptr;

  HIRUnaryExpr(HIRType *ty, HIRUnaryOp o, HIRExpr *in, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::UnaryExpr, ty, s), op(o), operand(in) {}
};

enum class HIRBinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  Mod,

  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,

  LogicalAnd,
  LogicalOr,

  BitAnd,
  BitOr,
  BitXor,
  Shl,
  Shr,
};

struct HIRBinaryExpr : HIRValueExpr {
  HIRBinaryOp op;
  HIRExpr *left = nullptr;
  HIRExpr *right = nullptr;

  HIRBinaryExpr(HIRType *ty, HIRBinaryOp o, HIRExpr *l, HIRExpr *r,
                SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::BinaryExpr, ty, s), op(o), left(l), right(r) {
  }
};

struct HIRCastExpr : HIRValueExpr {
  HIRExpr *value = nullptr;
  HIRType *fromType = nullptr;
  HIRType *toType = nullptr;

  HIRCastExpr(HIRExpr *v, HIRType *from, HIRType *to, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::CastExpr, to, s), value(v), fromType(from),
        toType(to) {}
};

struct HIRMethod {
  int id = -1;
  std::string name;
  HIRType *returnType = nullptr;
  MethodSymbol *symbol = nullptr;
  std::vector<std::unique_ptr<HIRParam>> params;

  bool isInit = false;
  bool isStatic = false;
  bool isAsync = false;
};

struct HIRCallExpr : HIRValueExpr {
  HIRMethod *callee = nullptr;
  std::vector<HIRExpr *> args;

  HIRCallExpr(HIRMethod *m, std::vector<HIRExpr *> a, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::CallExpr, m->returnType, s), callee(m),
        args(std::move(a)) {}
};

enum class HIRReceiverMode {
  Value,
  Observer,
};

struct HIRMethodCallExpr : HIRValueExpr {
  HIRExpr *receiver = nullptr;
  HIRMethod *method = nullptr;
  std::vector<HIRExpr *> args;
  HIRReceiverMode receiverMode = HIRReceiverMode::Value;

  HIRMethodCallExpr(HIRExpr *recv, HIRMethod *m, std::vector<HIRExpr *> a,
                    HIRReceiverMode rm, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::MethodCallExpr, m->returnType, s),
        receiver(recv), method(m), args(std::move(a)), receiverMode(rm) {}
};

struct HIRSpawnExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  HIREntityType *entityType = nullptr;
  HIRMethod *initMethod = nullptr; // 없으면 기본 생성 의미
  std::vector<HIRExpr *> args;

  HIRSpawnExpr(HIRHandleType *outType, StorageKind st, HIREntityType *ent,
               HIRMethod *init, std::vector<HIRExpr *> a, SourceSpan s = {})
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
