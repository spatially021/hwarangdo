#pragma once

#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SourceSpan.h"
#include "enums/Operator.h"
#include "vector"
#include <memory>
#include <utility>
#include <vector>

struct HIRMethodDecl;

using std::unique_ptr;

struct HIRMethodDecl;

using std::unique_ptr;

enum class HIRExprCategory {
  Value,
  Place,
};

struct HIRExpr : HIRNode {
  HIRType *type = nullptr;
  HIRExprCategory category = HIRExprCategory::Value;

  HIRExpr(SourceSpan s, HIRNodeKind k, HIRType *ty, HIRExprCategory c)
      : HIRNode(s, k), type(ty), category(c) {}
  virtual ~HIRExpr() = default;
};

struct HIRPlaceExpr : HIRExpr {
  HIRPlaceExpr(SourceSpan s, HIRNodeKind k, HIRType *ty)
      : HIRExpr(s, k, ty, HIRExprCategory::Place) {}
};

struct HIRValueExpr : HIRExpr {
  HIRValueExpr(SourceSpan s, HIRNodeKind k, HIRType *ty)
      : HIRExpr(s, k, ty, HIRExprCategory::Value) {}
};

struct HIRVaraintValueExpr : HIRValueExpr {

  HIREnumVariant *varaint = nullptr;
  std::unique_ptr<HIRExpr> payload = nullptr;
  HIRVaraintValueExpr(SourceSpan s, HIRType *t, HIREnumVariant *v,
                      std::unique_ptr<HIRExpr> p = nullptr)
      : HIRValueExpr(s, HIRNodeKind::EnumVariantValue, t), varaint(v),
        payload(std::move(p)) {}
};

struct HIRLocalPlaceExpr : HIRPlaceExpr {
  HIRLocal *local = nullptr;

  HIRLocalPlaceExpr(SourceSpan s, HIRLocal *l)
      : HIRPlaceExpr(s, HIRNodeKind::LocalPlaceExpr, l->type), local(l) {}
};

struct HIRParamPlaceExpr : HIRPlaceExpr {
  HIRParam *param = nullptr;

  HIRParamPlaceExpr(SourceSpan s, HIRParam *p)
      : HIRPlaceExpr(s, HIRNodeKind::ParamPlaceExpr, p->type), param(p) {}
};

struct HIRArrayAccessPlaceExpr : HIRPlaceExpr {
  unique_ptr<HIRPlaceExpr> object = nullptr;
  unique_ptr<HIRValueExpr> index = nullptr;
  HIRType *elementType = nullptr;

  HIRArrayAccessPlaceExpr(SourceSpan s, unique_ptr<HIRPlaceExpr> o,
                          unique_ptr<HIRValueExpr> i, HIRType *et)
      : HIRPlaceExpr(s, HIRNodeKind::ArrayAccessExpr, et), object(std::move(o)),
        index(std::move(i)), elementType(et) {}
};

enum class HIRSelfKind { This, Super, Self };

struct HIRSelfExpr : HIRValueExpr {
  HIRSelfKind selfKind;
  HIRType *ownerType = nullptr;
  HIRType *accessType = nullptr;

  HIRSelfExpr(SourceSpan s, HIRSelfKind k, HIRType *exprType, HIRType *owner,
              HIRType *access)
      : HIRValueExpr(s, HIRNodeKind::SelfExpr, exprType), selfKind(k),
        ownerType(owner), accessType(access) {}
};

struct HIRRootExpr : HIRValueExpr {
  HIRRootExpr(SourceSpan s, HIRType *t)
      : HIRValueExpr(s, HIRNodeKind::RootExpr, t) {}
};

struct HIRFieldPlaceExpr : HIRPlaceExpr {
  unique_ptr<HIRValueExpr> receiver = nullptr; // value or observer or root
  HIRField *field = nullptr;

  HIRFieldPlaceExpr(SourceSpan s, unique_ptr<HIRValueExpr> obj, HIRField *f)
      : HIRPlaceExpr(s, HIRNodeKind::FieldPlaceExpr, f->type),
        receiver(std::move(obj)), field(f) {}
};

struct HIRRecieverExpr : HIRPlaceExpr {};

struct HIRLiteralExpr : HIRValueExpr {
  ResolvedLit resolvedLit;

  HIRLiteralExpr(SourceSpan s, HIRType *ty, ResolvedLit rl)
      : HIRValueExpr(s, HIRNodeKind::LiteralExpr, ty), resolvedLit(rl) {}
};

struct HIRLoadExpr : HIRValueExpr {
  unique_ptr<HIRPlaceExpr> place = nullptr;

  HIRLoadExpr(SourceSpan s, unique_ptr<HIRPlaceExpr> p)
      : HIRValueExpr(s, HIRNodeKind::LoadExpr, p->type), place(std::move(p)) {}
};

struct HIRUnaryExpr : HIRValueExpr {
  Operator op;
  unique_ptr<HIRExpr> operand = nullptr;

  HIRUnaryExpr(SourceSpan s, HIRType *ty, Operator o, unique_ptr<HIRExpr> in)
      : HIRValueExpr(s, HIRNodeKind::UnaryExpr, ty), op(o),
        operand(std::move(in)) {}
};

struct HIRBinaryExpr : HIRValueExpr {
  Operator op;
  unique_ptr<HIRValueExpr> left = nullptr;
  unique_ptr<HIRValueExpr> right = nullptr;

  HIRBinaryExpr(SourceSpan s, HIRType *ty, Operator o,
                unique_ptr<HIRValueExpr> l, unique_ptr<HIRValueExpr> r)
      : HIRValueExpr(s, HIRNodeKind::BinaryExpr, ty), op(o), left(std::move(l)),
        right(std::move(r)) {}
};

struct HIRTernaryExpr : HIRValueExpr {
  std::unique_ptr<HIRValueExpr> condition;
  std::unique_ptr<HIRValueExpr> thenExpr;
  std::unique_ptr<HIRValueExpr> elseExpr;

  HIRTernaryExpr(SourceSpan s, std::unique_ptr<HIRValueExpr> cond,
                 std::unique_ptr<HIRValueExpr> thenE,
                 std::unique_ptr<HIRValueExpr> elseE, HIRType *resultType)
      : HIRValueExpr(s, HIRNodeKind::TernaryExpr, resultType),
        condition(std::move(cond)), thenExpr(std::move(thenE)),
        elseExpr(std::move(elseE)) {}
};

struct HIRCastExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> operand = nullptr;
  HIRType *fromType = nullptr;
  HIRType *toType = nullptr;

  HIRCastExpr(SourceSpan s, unique_ptr<HIRValueExpr> o, HIRType *from,
              HIRType *to)
      : HIRValueExpr(s, HIRNodeKind::CastExpr, to), operand(std::move(o)),
        fromType(from), toType(to) {}
};

struct HIRMethodCallExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> receiver = nullptr;
  HIRMethodDecl *method = nullptr;
  std::vector<std::unique_ptr<HIRExpr>> args;

  HIRMethodCallExpr(SourceSpan s, unique_ptr<HIRValueExpr> recv,
                    HIRMethodDecl *m, std::vector<std::unique_ptr<HIRExpr>> a,
                    HIRType *r)
      : HIRValueExpr(s, HIRNodeKind::MethodCallExpr, r),
        receiver(std::move(recv)), method(m), args(std::move(a)) {}
};

struct HIRStructInitExpr : HIRValueExpr {
  HIRMethodDecl *method = nullptr;
  std::vector<std::unique_ptr<HIRExpr>> args;
  bool isDefault = false;
  HIRBlockStmt *defaultInit = nullptr;
  HIRStructInitExpr(SourceSpan s, HIRMethodDecl *m,
                    std::vector<std::unique_ptr<HIRExpr>> a, HIRType *r,
                    HIRBlockStmt *de, bool d = false)
      : HIRValueExpr(s, HIRNodeKind::StructInitExpr, r), method(m),
        args(std::move(a)), isDefault(d), defaultInit(de) {}
};

struct HIRSpawnExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  HIREntityType *entityType = nullptr;
  HIRMethodDecl *initMethod = nullptr; // 없으면 기본 생성 의미
  std::vector<unique_ptr<HIRExpr>> args;

  HIRSpawnExpr(SourceSpan s, HIRHandleType *outType, StorageKind st,
               HIREntityType *ent, HIRMethodDecl *init,
               std::vector<unique_ptr<HIRExpr>> a)
      : HIRValueExpr(s, HIRNodeKind::SpawnExpr, outType), storage(st),
        entityType(ent), initMethod(init), args(std::move(a)) {}
};

struct HIRViewExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  unique_ptr<HIRValueExpr> handle = nullptr;
  HIREntityType *entityType = nullptr;

  HIRViewExpr(SourceSpan s, HIRObserverType *outType, StorageKind st,
              unique_ptr<HIRValueExpr> h, HIREntityType *ent)
      : HIRValueExpr(s, HIRNodeKind::ViewExpr, outType), storage(st),
        handle(std::move(h)), entityType(ent) {}
};

struct HIRMatchExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> cond = nullptr;
  vector<unique_ptr<HIRCase>> cases;

  HIRMatchExpr(SourceSpan s, HIRType *ty, unique_ptr<HIRValueExpr> c,
               vector<unique_ptr<HIRCase>> ca)
      : HIRValueExpr(s, HIRNodeKind::MatchExpr, ty), cond(std::move(c)),
        cases(std::move(ca)) {}
};

struct HIRDefaultValueExpr : HIRValueExpr {
  HIRDefaultValueExpr(SourceSpan s, HIRType *ty)
      : HIRValueExpr(s, HIRNodeKind::DefaultValueExpr, ty) {}
};
