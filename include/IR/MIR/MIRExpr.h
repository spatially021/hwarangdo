#pragma once

#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "enums/Operator.h"
#include "util/Error.h"
#include <memory>
#include <utility>
#include <vector>
struct MIRValue {
  virtual ~MIRValue() = default;
  virtual unique_ptr<MIRValue> clone() const = 0;
};

struct MIRPlace {
  ValueSymbol *symbol = nullptr;

  explicit MIRPlace(ValueSymbol *s) : symbol(s) {}
  virtual ~MIRPlace() = default;

  virtual std::unique_ptr<MIRPlace> clone() const = 0;
};

struct MIRLocalPlace final : MIRPlace {
  explicit MIRLocalPlace(ValueSymbol *s) : MIRPlace(s) {}
  std::unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRLocalPlace>(symbol);
  }
};

struct MIRParamPlace final : MIRPlace {
  explicit MIRParamPlace(ValueSymbol *s) : MIRPlace(s) {}
  unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRParamPlace>(symbol);
  }
};

struct MIRArrayAccessPlace final : MIRPlace {
  unique_ptr<MIRPlace> base = nullptr;
  unique_ptr<MIRValue> index = nullptr;
  explicit MIRArrayAccessPlace(unique_ptr<MIRPlace> b, unique_ptr<MIRValue> i)
      : MIRPlace(nullptr), base(std::move(b)), index(std::move(i)) {}
  unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRArrayAccessPlace>(base->clone(), index->clone());
  }
};

struct MIRFieldPlace final : MIRPlace {
  unique_ptr<MIRPlace> base = nullptr;
  MIRFieldPlace(ValueSymbol *s, unique_ptr<MIRPlace> b)
      : MIRPlace(s), base(std::move(b)) {}
  unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRFieldPlace>(symbol, base->clone());
  }
};

struct MIRRootPlace final : MIRPlace {
  MIRRootPlace() : MIRPlace(nullptr) {}
  unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRRootPlace>();
  }
};

struct MIRBinaryExpr : MIRValue {
  unique_ptr<MIRValue> lhs = nullptr;
  unique_ptr<MIRValue> rhs = nullptr;
  Operator op;
  MIRBinaryExpr(unique_ptr<MIRValue> l, unique_ptr<MIRValue> r, Operator o)
      : lhs(std::move(l)), rhs(std::move(r)), op(o) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRBinaryExpr>(lhs->clone(), rhs->clone(), op);
  }
};

struct MIRLoad : MIRValue {
  unique_ptr<MIRPlace> place = nullptr;
  MIRLoad(unique_ptr<MIRPlace> p) : place(std::move(p)) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRLoad>(place->clone());
  }
};

struct MIRPayloadExtractExpr : MIRValue {
  EnumVariantSymbol *symbol = nullptr;
  unique_ptr<MIRValue> enumValue;
  MIRPayloadExtractExpr(EnumVariantSymbol *s, unique_ptr<MIRValue> e)
      : symbol(s), enumValue(std::move(e)) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRPayloadExtractExpr>(symbol, enumValue->clone());
  }
};

struct MIRLiteralExpr : MIRValue {
  ResolvedLit literal;
  MIRLiteralExpr(ResolvedLit l) : literal(l) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRLiteralExpr>(literal);
  }
};

struct MIRUnaryExpr : MIRValue {
  unique_ptr<MIRValue> operrand = nullptr;
  Operator op;
  MIRUnaryExpr(unique_ptr<MIRValue> o, Operator oper)
      : operrand(std::move(o)), op(oper) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRUnaryExpr>(operrand->clone(), op);
  }
};

struct MIRCastExpr : MIRValue {
  unique_ptr<MIRValue> operrand = nullptr;
  TypeSymbol *from = nullptr;
  TypeSymbol *to = nullptr;
  MIRCastExpr(unique_ptr<MIRValue> o, TypeSymbol *f, TypeSymbol *t)
      : operrand(std::move(o)), from(f), to(t) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRCastExpr>(operrand->clone(), from, to);
  }
};

struct MIRCallExpr : MIRValue {
  std::unique_ptr<MIRValue> base = nullptr;
  vector<unique_ptr<MIRValue>> args;
  MethodSymbol *method = nullptr;
  MIRCallExpr(unique_ptr<MIRValue> b, vector<unique_ptr<MIRValue>> a,
              MethodSymbol *m)
      : base(std::move(b)), args(std::move(a)), method(m) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRCallExpr>(base->clone(), std::move(ar), method);
  }
};

struct MIRSpawnExpr : MIRValue {
  TypeSymbol *entityType = nullptr;
  MethodSymbol *initMethod = nullptr;
  std::vector<unique_ptr<MIRValue>> args;
  MIRSpawnExpr(TypeSymbol *e, MethodSymbol *i, vector<unique_ptr<MIRValue>> a)
      : entityType(e), initMethod(i), args(std::move(a)) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRSpawnExpr>(entityType, initMethod, std::move(ar));
  }
};

struct MIRViewExpr : MIRValue {
  TypeSymbol *entityType = nullptr;
  unique_ptr<MIRValue> handle = nullptr;
  MIRViewExpr(TypeSymbol *e, unique_ptr<MIRValue> h)
      : entityType(e), handle(std::move(h)) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRViewExpr>(entityType, handle->clone());
  }
};

struct MIRStructInitExpr : MIRValue {
  TypeSymbol *structType = nullptr;
  MethodSymbol *initMethod = nullptr;
  std::vector<unique_ptr<MIRValue>> args;
  MIRStructInitExpr(TypeSymbol *s, MethodSymbol *i,
                    vector<unique_ptr<MIRValue>> a)
      : structType(s), initMethod(i), args(std::move(a)) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRStructInitExpr>(structType, initMethod,
                                          std::move(ar));
  }
};

struct MIRVariantExpr : MIRValue {
  EnumVariantSymbol *variant = nullptr;
  unique_ptr<MIRValue> payload = nullptr;
  MIRVariantExpr(EnumVariantSymbol *v, unique_ptr<MIRValue> p)
      : variant(v), payload(std::move(p)) {
    if ((payload && !variant->isPayload) ||
        (payload == nullptr && variant->isPayload)) {
      Error::internal("illegal variant use");
    }
  }
  unique_ptr<MIRValue> clone() const override {
    if (payload) {
      return make_unique<MIRVariantExpr>(variant, payload->clone());
    }
    return make_unique<MIRVariantExpr>(variant, nullptr);
  }
};
