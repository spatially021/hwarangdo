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

struct HIRVaraintValueExpr : HIRValueExpr {

  HIREnumVariant *varaint = nullptr;
  std::unique_ptr<HIRExpr> payload = nullptr;
  HIRVaraintValueExpr(HIRType *t, HIREnumVariant *v,
                      std::unique_ptr<HIRExpr> p = nullptr, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::EnumVairantValue, t, s), varaint(v),
        payload(std::move(p)) {}
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

struct HIRArrayAccessPlaceExpr : HIRPlaceExpr {
  unique_ptr<HIRValueExpr> object = nullptr;
  unique_ptr<HIRValueExpr> index = nullptr;
  HIRType *elementType = nullptr;

  HIRArrayAccessPlaceExpr(unique_ptr<HIRValueExpr> o,
                          unique_ptr<HIRValueExpr> i, HIRType *et,
                          SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::ArrayAccessExpr, o->type, s),
        object(std::move(o)), index(std::move(i)), elementType(et) {}
};

enum class HIRSelfKind { This, Super, Self };

struct HIRSelfExpr : HIRValueExpr {
  HIRSelfKind selfKind;
  HIRType *ownerType = nullptr;
  HIRType *accessType = nullptr;

  HIRSelfExpr(HIRSelfKind k, HIRType *exprType, HIRType *owner, HIRType *access,
              SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::SelfExpr, exprType, s), selfKind(k),
        ownerType(owner), accessType(access) {}
};

struct HIRRootExpr : HIRValueExpr {
  HIRRootExpr(HIRType *t, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::RootExpr, t, s) {}
};

struct HIRFieldPlaceExpr : HIRPlaceExpr {
  unique_ptr<HIRValueExpr> receiver = nullptr; // value or observer
  HIRField *field = nullptr;

  HIRFieldPlaceExpr(unique_ptr<HIRValueExpr> obj, HIRField *f,
                    SourceSpan s = {})
      : HIRPlaceExpr(HIRNodeKind::FieldPlaceExpr, f->type, s),
        receiver(std::move(obj)), field(f) {}
};

struct HIRRecieverExpr : HIRPlaceExpr {};

struct HIRLiteralExpr : HIRValueExpr {
  ResolvedLit resolvedLit = {};

  HIRLiteralExpr(HIRType *ty, ResolvedLit rl, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::LiteralExpr, ty, s), resolvedLit(rl) {}
};

struct HIRLoadExpr : HIRValueExpr {
  unique_ptr<HIRPlaceExpr> place = nullptr;

  HIRLoadExpr(unique_ptr<HIRPlaceExpr> p, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::LoadExpr, p->type, s), place(std::move(p)) {}
};

struct HIRAssignExpr : HIRValueExpr {
  unique_ptr<HIRPlaceExpr> lhs = nullptr;
  unique_ptr<HIRValueExpr> rhs = nullptr;

  HIRAssignExpr(unique_ptr<HIRPlaceExpr> l, unique_ptr<HIRValueExpr> r,
                SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::AssignExpr, l->type, s), lhs(std::move(l)),
        rhs(std::move(r)) {}
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

struct HIRTernaryExpr : HIRValueExpr {
  std::unique_ptr<HIRValueExpr> condition;
  std::unique_ptr<HIRValueExpr> thenExpr;
  std::unique_ptr<HIRValueExpr> elseExpr;

  HIRTernaryExpr(std::unique_ptr<HIRValueExpr> cond,
                 std::unique_ptr<HIRValueExpr> thenE,
                 std::unique_ptr<HIRValueExpr> elseE, HIRType *resultType,
                 SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::TernaryExpr, resultType, s),
        condition(std::move(cond)), thenExpr(std::move(thenE)),
        elseExpr(std::move(elseE)) {}
};

struct HIRCastExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> operand = nullptr;
  HIRType *fromType = nullptr;
  HIRType *toType = nullptr;

  HIRCastExpr(unique_ptr<HIRValueExpr> o, HIRType *from, HIRType *to,
              SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::CastExpr, to, s), operand(std::move(o)),
        fromType(from), toType(to) {}
};

struct HIRMethodCallExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> receiver = nullptr;
  HIRMethodDecl *method = nullptr;
  std::vector<std::unique_ptr<HIRExpr>> args;

  HIRMethodCallExpr(unique_ptr<HIRValueExpr> recv, HIRMethodDecl *m,
                    std::vector<std::unique_ptr<HIRExpr>> a, HIRType *r,
                    SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::MethodCallExpr, r, s),
        receiver(std::move(recv)), method(m), args(std::move(a)) {}
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

  HIRViewExpr(HIRObserverType *outType, StorageKind st, HIRExpr *h,
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

struct HIRDefaultValueExpr : HIRValueExpr {
  HIRDefaultValueExpr(HIRType *ty, SourceSpan s = {})
      : HIRValueExpr(HIRNodeKind::DefaultValueExpr, ty, s) {}
};