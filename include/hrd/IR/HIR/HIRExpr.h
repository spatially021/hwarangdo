#pragma once

#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/enums/Operator.h"
#include "vector"
#include <memory>
#include <utility>
#include <vector>

struct HIRMethodDecl;

using std::unique_ptr;

enum class HIRExprCategory {
  Value,
  Place,
};

struct HIRExpr : HIRNode {
  TypeSymbol *type = nullptr;
  HIRExprCategory category = HIRExprCategory::Value;

  HIRExpr(SourceSpan s, HIRNodeKind k, TypeSymbol *ty, HIRExprCategory c)
      : HIRNode(s, k), type(ty), category(c) {}
  virtual ~HIRExpr() = default;
};

struct HIRPlaceExpr : HIRExpr {
  HIRPlaceExpr(SourceSpan s, HIRNodeKind k, TypeSymbol *ty)
      : HIRExpr(s, k, ty, HIRExprCategory::Place) {}
};

struct HIRValueExpr : HIRExpr {
  HIRValueExpr(SourceSpan s, HIRNodeKind k, TypeSymbol *ty)
      : HIRExpr(s, k, ty, HIRExprCategory::Value) {}
};

struct HIRVariantValueExpr : HIRValueExpr {

  EnumVariantSymbol *varaint = nullptr;
  std::unique_ptr<HIRExpr> payload = nullptr;
  HIRVariantValueExpr(SourceSpan s, TypeSymbol *t, EnumVariantSymbol *v,
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
  TypeSymbol *elementType = nullptr;

  HIRArrayAccessPlaceExpr(SourceSpan s, unique_ptr<HIRPlaceExpr> o,
                          unique_ptr<HIRValueExpr> i, TypeSymbol *et)
      : HIRPlaceExpr(s, HIRNodeKind::ArrayAccessExpr, et), object(std::move(o)),
        index(std::move(i)), elementType(et) {}
};

enum class HIRSelfKind { This, Super, Self };

struct HIRSelfExpr : HIRValueExpr {
  HIRSelfKind selfKind;
  TypeSymbol *ownerType = nullptr;
  TypeSymbol *accessType = nullptr;

  HIRSelfExpr(SourceSpan s, HIRSelfKind k, TypeSymbol *exprType,
              TypeSymbol *owner, TypeSymbol *access)
      : HIRValueExpr(s, HIRNodeKind::SelfExpr, exprType), selfKind(k),
        ownerType(owner), accessType(access) {}
};

struct HIRRootExpr : HIRValueExpr {
  HIRRootExpr(SourceSpan s, TypeSymbol *t)
      : HIRValueExpr(s, HIRNodeKind::RootExpr, t) {}
};

struct HIRFieldPlaceExpr : HIRPlaceExpr {
  unique_ptr<HIRValueExpr> receiver = nullptr; // value or observer or root
  ValueSymbol *field = nullptr;

  HIRFieldPlaceExpr(SourceSpan s, unique_ptr<HIRValueExpr> obj, ValueSymbol *f)
      : HIRPlaceExpr(s, HIRNodeKind::FieldPlaceExpr, f->typeSymbol),
        receiver(std::move(obj)), field(f) {}
};

struct HIRRecieverExpr : HIRPlaceExpr {};

struct HIRLiteralExpr : HIRValueExpr {
  ResolvedLit resolvedLit;

  HIRLiteralExpr(SourceSpan s, TypeSymbol *ty, ResolvedLit rl)
      : HIRValueExpr(s, HIRNodeKind::LiteralExpr, ty), resolvedLit(rl) {}
};

struct HIRArrayLiteralExpr : HIRValueExpr {
  TypeSymbol *elementType = nullptr;
  std::vector<std::unique_ptr<HIRExpr>> elements;
  HIRArrayLiteralExpr(SourceSpan s, TypeSymbol *ty, TypeSymbol *et,
                      std::vector<std::unique_ptr<HIRExpr>> e)
      : HIRValueExpr(s, HIRNodeKind::ArrayLiteralExpr, ty), elementType(et),
        elements(std::move(e)) {}
};

struct HIRLoadExpr : HIRValueExpr {
  unique_ptr<HIRPlaceExpr> place = nullptr;

  HIRLoadExpr(SourceSpan s, unique_ptr<HIRPlaceExpr> p)
      : HIRValueExpr(s, HIRNodeKind::LoadExpr, p->type), place(std::move(p)) {}
};

struct HIRUnaryExpr : HIRValueExpr {
  Operator op;
  unique_ptr<HIRExpr> operand = nullptr;

  HIRUnaryExpr(SourceSpan s, TypeSymbol *ty, Operator o, unique_ptr<HIRExpr> in)
      : HIRValueExpr(s, HIRNodeKind::UnaryExpr, ty), op(o),
        operand(std::move(in)) {}
};

struct HIRBinaryExpr : HIRValueExpr {
  Operator op;
  unique_ptr<HIRValueExpr> left = nullptr;
  unique_ptr<HIRValueExpr> right = nullptr;
  TypeSymbol *operand = nullptr;
  HIRBinaryExpr(SourceSpan s, TypeSymbol *ty, Operator o,
                unique_ptr<HIRValueExpr> l, unique_ptr<HIRValueExpr> r,
                TypeSymbol *ope)
      : HIRValueExpr(s, HIRNodeKind::BinaryExpr, ty), op(o), left(std::move(l)),
        right(std::move(r)), operand(ope) {}
};

struct HIRTernaryExpr : HIRValueExpr {
  std::unique_ptr<HIRValueExpr> condition;
  std::unique_ptr<HIRValueExpr> thenExpr;
  std::unique_ptr<HIRValueExpr> elseExpr;

  HIRTernaryExpr(SourceSpan s, std::unique_ptr<HIRValueExpr> cond,
                 std::unique_ptr<HIRValueExpr> thenE,
                 std::unique_ptr<HIRValueExpr> elseE, TypeSymbol *resultType)
      : HIRValueExpr(s, HIRNodeKind::TernaryExpr, resultType),
        condition(std::move(cond)), thenExpr(std::move(thenE)),
        elseExpr(std::move(elseE)) {}
};

struct HIRCastExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> operand = nullptr;
  TypeSymbol *fromType = nullptr;
  TypeSymbol *toType = nullptr;

  HIRCastExpr(SourceSpan s, unique_ptr<HIRValueExpr> o, TypeSymbol *from,
              TypeSymbol *to)
      : HIRValueExpr(s, HIRNodeKind::CastExpr, to), operand(std::move(o)),
        fromType(from), toType(to) {}
};

struct HIRMethodCallExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> receiver = nullptr;
  MethodSymbol *method = nullptr;
  std::vector<std::unique_ptr<HIRExpr>> args;

  HIRMethodCallExpr(SourceSpan s, unique_ptr<HIRValueExpr> recv,
                    MethodSymbol *m, std::vector<std::unique_ptr<HIRExpr>> a,
                    TypeSymbol *r)
      : HIRValueExpr(s, HIRNodeKind::MethodCallExpr, r),
        receiver(std::move(recv)), method(m), args(std::move(a)) {}
};

struct HIRRuntimeCall : HIRValueExpr {
  RuntimeSymbol *symbol = nullptr;
  vector<unique_ptr<HIRExpr>> args;
  HIRRuntimeCall(SourceSpan s, RuntimeSymbol *r, vector<unique_ptr<HIRExpr>> a,
                 TypeSymbol *t)
      : HIRValueExpr(s, HIRNodeKind::RuntimeCallExpr, t), symbol(r),
        args(std::move(a)) {}
};

struct HIRStructInitExpr : HIRValueExpr {
  MethodSymbol *method = nullptr;
  std::vector<std::unique_ptr<HIRExpr>> args;
  bool isDefault = false;
  HIRStructInitExpr(SourceSpan s, MethodSymbol *m,
                    std::vector<std::unique_ptr<HIRExpr>> a, TypeSymbol *r,
                    bool d = false)
      : HIRValueExpr(s, HIRNodeKind::StructInitExpr, r), method(m),
        args(std::move(a)), isDefault(d) {}
};

struct HIRSpawnExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  TypeSymbol *entityType = nullptr;
  HIRMethodDecl *initMethod = nullptr; // 없으면 기본 생성 의미
  std::vector<unique_ptr<HIRExpr>> args;

  HIRSpawnExpr(SourceSpan s, TypeSymbol *outType, StorageKind st,
               TypeSymbol *ent, HIRMethodDecl *init,
               std::vector<unique_ptr<HIRExpr>> a)
      : HIRValueExpr(s, HIRNodeKind::SpawnExpr, outType), storage(st),
        entityType(ent), initMethod(init), args(std::move(a)) {}
};

struct HIRViewExpr : HIRValueExpr {
  StorageKind storage = StorageKind::World;
  unique_ptr<HIRValueExpr> handle = nullptr;
  TypeSymbol *entityType = nullptr;

  HIRViewExpr(SourceSpan s, TypeSymbol *outType, StorageKind st,
              unique_ptr<HIRValueExpr> h, TypeSymbol *ent)
      : HIRValueExpr(s, HIRNodeKind::ViewExpr, outType), storage(st),
        handle(std::move(h)), entityType(ent) {}
};

struct HIRMatchExpr : HIRValueExpr {
  unique_ptr<HIRValueExpr> cond = nullptr;
  vector<unique_ptr<HIRCase>> cases;

  HIRMatchExpr(SourceSpan s, TypeSymbol *ty, unique_ptr<HIRValueExpr> c,
               vector<unique_ptr<HIRCase>> ca)
      : HIRValueExpr(s, HIRNodeKind::MatchExpr, ty), cond(std::move(c)),
        cases(std::move(ca)) {}
};

struct HIRDefaultValueExpr : HIRValueExpr {
  HIRDefaultValueExpr(SourceSpan s, TypeSymbol *ty)
      : HIRValueExpr(s, HIRNodeKind::DefaultValueExpr, ty) {}
};
