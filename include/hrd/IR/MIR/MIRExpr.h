#pragma once

#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/enums/Operator.h"
#include <memory>
#include <utility>
#include <vector>
enum class MIRValueCategory {
  Plain,
  Borrowed,
  OwnedTemp,
};

struct MIRValue {
  TypeSymbol *type = nullptr;
  MIRValueCategory valueCategory = MIRValueCategory::Plain;
  MIRValue(TypeSymbol *t) : type(t) {
    if (dynamic_cast<StringType *>(type)) {
      valueCategory = MIRValueCategory::Borrowed;
    }
  }
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

  TypeSymbol *ownType = nullptr; // 인덱싱 대상 배열 타입

  explicit MIRArrayAccessPlace(unique_ptr<MIRPlace> b, unique_ptr<MIRValue> i,
                               ValueSymbol *s, TypeSymbol *own)
      : MIRPlace(s), base(std::move(b)), index(std::move(i)), ownType(own) {}

  unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRArrayAccessPlace>(base->clone(), index->clone(),
                                            symbol, ownType);
  }
};
struct MIRFieldPlace final : MIRPlace {
  unique_ptr<MIRPlace> base = nullptr;
  TypeSymbol *ownType = nullptr;
  MIRFieldPlace(ValueSymbol *s, unique_ptr<MIRPlace> b, TypeSymbol *o)
      : MIRPlace(s), base(std::move(b)), ownType(o) {}
  unique_ptr<MIRPlace> clone() const override {
    return make_unique<MIRFieldPlace>(symbol, base->clone(), ownType);
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
  TypeSymbol *operandType = nullptr;
  MIRBinaryExpr(unique_ptr<MIRValue> l, unique_ptr<MIRValue> r, Operator o,
                TypeSymbol *t, TypeSymbol *ope)
      : MIRValue(t), lhs(std::move(l)), rhs(std::move(r)), op(o),
        operandType(ope) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRBinaryExpr>(lhs->clone(), rhs->clone(), op, type,
                                      operandType);
  }
};

struct MIRLoad : MIRValue {
  unique_ptr<MIRPlace> place = nullptr;
  MIRLoad(unique_ptr<MIRPlace> p, TypeSymbol *t)
      : MIRValue(t), place(std::move(p)) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRLoad>(place->clone(), type);
  }
};

struct MIRPayloadExtractExpr : MIRValue {
  EnumVariantSymbol *symbol = nullptr;
  unique_ptr<MIRValue> enumValue;
  MIRPayloadExtractExpr(EnumVariantSymbol *s, unique_ptr<MIRValue> e,
                        TypeSymbol *t)
      : MIRValue(t), symbol(s), enumValue(std::move(e)) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRPayloadExtractExpr>(symbol, enumValue->clone(), type);
  }
};

struct MIRLiteralExpr : MIRValue {
  ResolvedLit literal;
  MIRLiteralExpr(ResolvedLit l, TypeSymbol *t) : MIRValue(t), literal(l) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRLiteralExpr>(literal, type);
  }
};

struct MIRArrayInitExpr : MIRValue {
  MIRArrayInitExpr(TypeSymbol *t, TypeSymbol *e,
                   std::vector<std::unique_ptr<MIRValue>> el)
      : MIRValue(t), elements(std::move(el)), elementType(e) {}

  std::vector<std::unique_ptr<MIRValue>> elements;
  TypeSymbol *elementType = nullptr;
  unique_ptr<MIRValue> clone() const override {
    std::vector<std::unique_ptr<MIRValue>> el;
    for (auto &e : elements) {
      el.push_back(e->clone());
    }

    return make_unique<MIRArrayInitExpr>(type, elementType, std::move(el));
  }
};

struct MIRUnaryExpr : MIRValue {
  unique_ptr<MIRValue> operrand = nullptr;
  Operator op;
  MIRUnaryExpr(unique_ptr<MIRValue> o, Operator oper, TypeSymbol *t)
      : MIRValue(t), operrand(std::move(o)), op(oper) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRUnaryExpr>(operrand->clone(), op, type);
  }
};

struct MIRCastExpr : MIRValue {
  unique_ptr<MIRValue> operrand = nullptr;
  TypeSymbol *from = nullptr;
  TypeSymbol *to = nullptr;
  MIRCastExpr(unique_ptr<MIRValue> o, TypeSymbol *f, TypeSymbol *t)
      : MIRValue(t), operrand(std::move(o)), from(f), to(t) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRCastExpr>(operrand->clone(), from, to);
  }
};

struct MIRCallExpr : MIRValue {
  std::unique_ptr<MIRValue> base = nullptr;
  vector<unique_ptr<MIRValue>> args;
  MethodSymbol *method = nullptr;
  MIRCallExpr(unique_ptr<MIRValue> b, vector<unique_ptr<MIRValue>> a,
              MethodSymbol *m, TypeSymbol *t)
      : MIRValue(t), base(std::move(b)), args(std::move(a)), method(m) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRCallExpr>(base->clone(), std::move(ar), method, type);
  }
};

struct MIRSpawnExpr : MIRValue {
  TypeSymbol *entityType = nullptr;
  MethodSymbol *initMethod = nullptr;
  std::vector<unique_ptr<MIRValue>> args;
  MIRSpawnExpr(TypeSymbol *e, MethodSymbol *i, vector<unique_ptr<MIRValue>> a,
               TypeSymbol *t)
      : MIRValue(t), entityType(e), initMethod(i), args(std::move(a)) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRSpawnExpr>(entityType, initMethod, std::move(ar),
                                     type);
  }
};

struct MIRViewExpr : MIRValue {
  TypeSymbol *entityType = nullptr;
  unique_ptr<MIRValue> handle = nullptr;
  MIRViewExpr(TypeSymbol *e, unique_ptr<MIRValue> h, TypeSymbol *t)
      : MIRValue(t), entityType(e), handle(std::move(h)) {}
  unique_ptr<MIRValue> clone() const override {
    return make_unique<MIRViewExpr>(entityType, handle->clone(), type);
  }
};

struct MIRStructInitExpr : MIRValue {
  TypeSymbol *structType = nullptr;
  MethodSymbol *initMethod = nullptr;
  std::vector<unique_ptr<MIRValue>> args;

  MIRStructInitExpr(TypeSymbol *s, MethodSymbol *i,
                    vector<unique_ptr<MIRValue>> a, TypeSymbol *t)
      : MIRValue(t), structType(s), initMethod(i), args(std::move(a)) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRStructInitExpr>(structType, initMethod, std::move(ar),
                                          type);
  }
};

struct MIRVariantExpr : MIRValue {
  EnumVariantSymbol *variant = nullptr;
  unique_ptr<MIRValue> payload = nullptr;
  MIRVariantExpr(EnumVariantSymbol *v, unique_ptr<MIRValue> p, TypeSymbol *t)
      : MIRValue(t), variant(v), payload(std::move(p)) {}
  unique_ptr<MIRValue> clone() const override {
    if (payload) {
      return make_unique<MIRVariantExpr>(variant, payload->clone(), type);
    }
    return make_unique<MIRVariantExpr>(variant, nullptr, type);
  }
};

struct MIRRuntimeCallExpr : MIRValue {
  RuntimeSymbol *symbol;
  vector<unique_ptr<MIRValue>> args;
  MIRRuntimeCallExpr(RuntimeSymbol *s, vector<unique_ptr<MIRValue>> a,
                     TypeSymbol *t)
      : MIRValue(t), symbol(s), args(std::move(a)) {}
  unique_ptr<MIRValue> clone() const override {
    vector<unique_ptr<MIRValue>> ar;
    for (auto &a : args) {
      ar.push_back(a->clone());
    }
    return make_unique<MIRRuntimeCallExpr>(symbol, std::move(ar), type);
  }
};
