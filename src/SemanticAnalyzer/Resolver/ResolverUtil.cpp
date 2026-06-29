#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Expr.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/util/Error.h"
#include <cassert>

ValueSymbol *Resolver::resolveValue(str name) {
  if (auto v = table->getValue(name))
    return v;
  if (currentSelf && currentSelf->value.find(name) != currentSelf->value.end())
    return currentSelf->value.find(name)->second.get();

  return nullptr;
}

ValueSymbol *Resolver::lookLocalValue(str name, Scope *localScope) {
  if (localScope->value.find(name) != localScope->value.end())
    return localScope->value.find(name)->second.get();
  return nullptr;
}

bool Resolver::isAssignable(TypeSymbol *from, TypeSymbol *to) {
  return canImplicitlyConvert(from, to).first;
}

bool Resolver::isBinaryOperatalbe(Operator op, TypeSymbol *left,
                                  TypeSymbol *right) {

  assert(left != nullptr);
  assert(right != nullptr);

  switch (op) {
  case Operator::B_AND:
  case Operator::B_OR:
  case Operator::B_XOR:
  case Operator::LSH:
  case Operator::RSH:
    return table->isInt(left) && table->isInt(right);

  case Operator::LS:
  case Operator::LSE:
  case Operator::GR:
  case Operator::GRE:
  case Operator::ADD:
  case Operator::SUB:
  case Operator::MUL:
  case Operator::DIV:
  case Operator::REM:
  case Operator::POW:
    return table->isNumberic(left) && table->isNumberic(right);

  case Operator::AND:
  case Operator::OR:
    return table->isBool(left) && table->isBool(right);

  case Operator::EQ:
  case Operator::NT:
    return isCmpable(left, right);
    break;

    break;
  case Operator::L_NOT:
  case Operator::B_NOT:
  case Operator::PLUS:
  case Operator::MINUS:
    Error::internal("Unmatched operator Type");
    break;
  }
  return false;
}

bool Resolver::isCmpable(TypeSymbol *left, TypeSymbol *right) {

  if (table->isNumberic(left) && table->isNumberic(right))
    return true;

  if (table->isBool(left) && table->isBool(right))
    return true;

  if (left->kind == TypeSymbol::TypeKind::ENUM &&
      right->kind == TypeSymbol::TypeKind::ENUM)
    return left == right;

  return left == right;
}

TypeSymbol *Resolver::binaryResult(Operator op, TypeSymbol *left,
                                   TypeSymbol *right) {
  assert(left != nullptr);
  assert(right != nullptr);

  switch (op) {
  case Operator::B_AND:
  case Operator::B_OR:
  case Operator::B_XOR:
  case Operator::LSH:
  case Operator::RSH:
  case Operator::ADD:
  case Operator::SUB:
  case Operator::MUL:
  case Operator::DIV:
  case Operator::REM:
  case Operator::POW:
    if (auto temp = binaryCasting(left, right)) {
      return temp;
    }
    Error::internal("fail to binary casting");

  case Operator::LS:
  case Operator::LSE:
  case Operator::GR:
  case Operator::GRE:
  case Operator::AND:
  case Operator::OR:
  case Operator::EQ:
  case Operator::NT:
    return table->getBuilt("bool");

  case Operator::L_NOT:
  case Operator::B_NOT:
  case Operator::PLUS:
  case Operator::MINUS:
    Error::internal("Unmatched operator Type");
    break;
  }
  return nullptr;
}

[[noreturn]]
void Resolver::unmatchSymbol(Symbol *symbol) {
  Error::internal(symbol->name + ": unmatched symbol");
}

bool Resolver::isCastable(TypeSymbol *from, TypeSymbol *to) {
  return from == to;
}
TypeSymbol *Resolver::binaryCasting(TypeSymbol *left, TypeSymbol *right) {

  if (left == right)
    return left;

  if ((left->kind != TypeSymbol::TypeKind::PRIMITIVE ||
       right->kind != TypeSymbol::TypeKind::PRIMITIVE)) {
    return nullptr;
  }

  vector<TypeSymbol *> candidates = getPromotionCandidates(left, right);
  for (auto *T : candidates) {
    if (canImplicitlyConvert(left, T).first &&
        canImplicitlyConvert(right, T).first)
      return T;
  }
  return nullptr;
}

vector<TypeSymbol *> Resolver::getPromotionCandidates(TypeSymbol *left,
                                                      TypeSymbol *right) {
  vector<TypeSymbol *> result;

  if (left->kind != TypeSymbol::TypeKind::PRIMITIVE ||
      right->kind != TypeSymbol::TypeKind::PRIMITIVE)
    return result;

  auto l = static_cast<PrimtiveType *>(left);
  auto r = static_cast<PrimtiveType *>(right);

  // 1️⃣ float이 있으면 float 후보 우선
  if (table->isFloat(l) || table->isFloat(r)) {

    if (table->isFloat(l))
      result.push_back(left);
    if (table->isFloat(r))
      result.push_back(right);

    return result;
  }

  // 2️⃣ 둘 다 int면 더 큰 쪽 먼저
  if (table->isInt(l) && table->isInt(r)) {

    auto bigger = (static_cast<IntType *>(l)->bitWidth >=
                   static_cast<IntType *>(r)->bitWidth)
                      ? left
                      : right;

    auto smaller = (bigger == left) ? right : left;

    result.push_back(bigger);
    result.push_back(smaller);

    return result;
  }

  return result;
}

pair<bool, CastingFailKind> Resolver::canImplicitlyConvert(TypeSymbol *from,
                                                           TypeSymbol *to) {

  if (!from) {
    Error::internal("from is nullptr");
  }
  if (!to) {
    Error::internal("to is nullptr");
  }
  for (auto p = from; p != nullptr; p = p->base) {
    if (p == to) {
      return {true, CastingFailKind::None};
    }
  }

  if (from->kind != TypeSymbol::TypeKind::PRIMITIVE ||
      to->kind != TypeSymbol::TypeKind::PRIMITIVE) {
    return {true, CastingFailKind::Unmatched};
  }

  auto f = static_cast<PrimtiveType *>(from);
  auto t = static_cast<PrimtiveType *>(to);

  if (table->isInt(f) && table->isInt(t)) {
    auto fi = static_cast<IntType *>(f);
    auto ti = static_cast<IntType *>(t);

    if (fi->isSigned == ti->isSigned)
      return {ti->bitWidth >= fi->bitWidth, CastingFailKind::Overflow};

    // signed → unsigned : 금지
    if (fi->isSigned && !ti->isSigned)
      return {false, CastingFailKind::SignToUnsign};

    // unsigned → signed
    if (!fi->isSigned && ti->isSigned)
      return {ti->bitWidth >= fi->bitWidth, CastingFailKind::Overflow};

    return {ti->bitWidth >= fi->bitWidth, CastingFailKind::Overflow};
  }

  if (table->isFloat(f) && table->isFloat(t)) {
    return {static_cast<FloatType *>(t)->bitWidth >=
                static_cast<FloatType *>(f)->bitWidth,
            CastingFailKind::Overflow};
  }

  if (table->isInt(f) && table->isFloat(t)) {

    auto fi = static_cast<IntType *>(f);
    auto tf = static_cast<FloatType *>(t);

    return {fi->bitWidth <= tf->precious, CastingFailKind::Overflow};
  }

  return {false, CastingFailKind::Unmatched};
}

const static llvm::fltSemantics &getFloatSemantics(FloatType *type) {

  switch (type->bitWidth) {
  case 16:
    return llvm::APFloat::IEEEhalf();

  case 32:
    return llvm::APFloat::IEEEsingle();

  case 64:
    return llvm::APFloat::IEEEdouble();

  case 128:
    return llvm::APFloat::IEEEquad();

  default:
    Error::internal("illegal float bitWidth");
  }
}

pair<bool, CastingFailKind>
Resolver::canImplicitlyLiteralConvert(LiteralExpr *from, TypeSymbol *to) {
  assert(from);
  assert(to);

  if (from->resolvedType == to) {
    return {true, CastingFailKind::None};
  }

  if (to->kind != TypeSymbol::TypeKind::PRIMITIVE) {
    return {false, CastingFailKind::Unmatched};
  }

  auto target = static_cast<PrimtiveType *>(to);

  switch (from->token.kind) {
  case TKind::LIT_INT: {
    if (!table->isInt(target) && !table->isFloat(target)) {
      return {false, CastingFailKind::InvalidCategory};
    }

    const auto &value = from->resolvedLit.asInt().value;

    if (table->isInt(target)) {
      auto ti = static_cast<IntType *>(target);
      if (ti->isSigned) {
        return {value.isSignedIntN(ti->bitWidth), CastingFailKind::Overflow};
      }

      if (value.isNegative()) {
        return {false, CastingFailKind::NegativeToUnsigned};
      }

      return {value.getActiveBits() <= ti->bitWidth, CastingFailKind::Overflow};
    }

    if (table->isFloat(target)) {
      auto tf = static_cast<FloatType *>(target);

      // 정수 리터럴 -> float은 "정확히 표현 가능"할 때만 허용
      // 보수적으로는 필요한 signed bits가 float precision 이하인지 검사
      return {value.getSignificantBits() <= tf->precious,
              CastingFailKind::PrecisionLoss};
    }

    return {false, CastingFailKind::Unmatched};
  }

  case TKind::LIT_FLOAT: {
    if (!table->isFloat(target)) {
      return {false, CastingFailKind::Unmatched};
    }

    auto tf = static_cast<FloatType *>(target);

    const auto &value = from->resolvedLit.asFloat().value;

    // 여기서 정확 판정은 APFloat 변환 후 losesInfo 검사로 처리하는 게 맞음.
    llvm::APFloat converted = value;
    bool losesInfo = false;

    converted.convert(getFloatSemantics(tf), llvm::APFloat::rmNearestTiesToEven,
                      &losesInfo);

    return {!losesInfo, CastingFailKind::FractionLoss};
  }

  case TKind::LIT_CHARACTER: {
    if (!table->isChar(target)) {
      return {false, CastingFailKind::Unmatched};
    }

    auto tc = static_cast<CharType *>(target);
    auto codePoint = from->resolvedLit.asChar().codePoint;

    switch (tc->bitWidth) {
    case 8:
      return {codePoint <= 0x7F, CastingFailKind::Overflow};
    case 16:
      return {codePoint <= 0xFFFF, CastingFailKind::Overflow};
    case 32:
      return {codePoint <= 0x10FFFF, CastingFailKind::Overflow};
    default:
      return {false, CastingFailKind::Overflow};
    }
  }

  case TKind::LIT_STRING: {
    if (!table->isString(target)) {
      return {false, CastingFailKind::Unmatched};
    }

    auto ts = static_cast<StringType *>(target);

    for (auto cp : from->resolvedLit.asString().codePoints) {
      switch (ts->bitWidth) {
      case 8:
        if (cp > 0x7F)
          return {false, CastingFailKind::Overflow};
        break;
      case 16:
        if (cp > 0xFFFF)
          return {false, CastingFailKind::Overflow};
        break;
      case 32:
        if (cp > 0x10FFFF)
          return {false, CastingFailKind::Overflow};
        break;
      default:
        return {false, CastingFailKind::Overflow};
      }
    }

    return {true, CastingFailKind::None};
  }

  case TKind::LIT_BOOL:
    return {to == table->getBool(), CastingFailKind::Unmatched};

  default:
    return {false, CastingFailKind::NotImplemented};
  }
}

pair<TypeSymbol *, CastingFailKind> Resolver::implicitCasting(Expr *from,
                                                              TypeSymbol *to) {
  if (auto lit = dynamic_cast<LiteralExpr *>(from)) {
    auto [result, kind] = canImplicitlyLiteralConvert(lit, to);
    if (result) {
      return {to, kind};
    }
    return {nullptr, kind};
  }
  auto [result, kind] = canImplicitlyConvert(from->resolvedType, to);
  if (result) {
    return {to, kind};
  }

  return {nullptr, kind};
}

ValueSymbol *Resolver::lookupEnumVariant(TypeSymbol *enumType,
                                         const string &name,
                                         SourceSpan &token) {
  if (!enumType || enumType->kind != TypeSymbol::TypeKind::ENUM) {
    Error::internal(token, "expected enum type");
  }

  auto it = enumType->variantMap.find(name);
  if (it == enumType->variantMap.end()) {
    Error::diagnostic(token, name + " is not " + enumType->name + "'s variant");
  }

  return it->second;
}

pair<bool, MethodSymbol *> Resolver::lookupMethod(str name, Scope *scope,
                                                  vector<TypeSymbol *> args) {

  auto &bucket = scope->methodMap[name];
  if (bucket.empty() && args.empty()) {
    return {true, nullptr};
  }
  for (auto &m : bucket) {
    if (m->params.size() != args.size()) {
      continue;
    }
    bool flag = true;
    for (unsigned int i = 0; i < args.size(); ++i) {
      if (m->params[i]->typeSymbol != args[i]) {
        flag = false;
        break;
      }
    }
    if (flag) {
      return {true, m};
    }
  }

  return {false, nullptr};
}

static unsigned minSignedBits(const llvm::APInt &v) {
  if (v.isNegative()) {
    // 음수는 ~v의 magnitude bits + sign bit
    return (~v).getActiveBits() + 1;
  }

  // 양수는 magnitude bits + sign bit
  return v.getActiveBits() + 1;
}

void Resolver::convertLit(LiteralExpr *lit, TypeNode *type) {

  auto prim = dynamic_cast<PrimtiveType *>(type->resolved);
  if (prim == nullptr) {
    Error::diagnostic(lit->span, "invalid init type");
  }

  switch (prim->builtinCategory) {
  case BuiltinCategory::Float: {
    if (lit->resolvedLit.isInt()) {
      llvm::APInt i = lit->resolvedLit.asInt().value;
      auto value = minSignedBits(i);
      if (value <= 24) {
        type->resolved = table->getType("f32");
        return;
      }
      if (value <= 53) {
        type->resolved = table->getType("f64");
        return;
      }
      if (value <= 113) {
        type->resolved = table->getType("f128");
        return;
      }
      Error::diagnostic(lit->span, "unsupported floating-point literal range");
    }
  } break;
  case BuiltinCategory::Int:
  case BuiltinCategory::Char:
  case BuiltinCategory::String:
  case BuiltinCategory::Bool:
  case BuiltinCategory::Void:
  case BuiltinCategory::Func:
  case BuiltinCategory::Fixed:
    break;
  }
}
void Resolver::inferencePrim(TypeNode *decl, TypeSymbol *init) {

  if (auto p = dynamic_cast<PrimtiveType *>(decl->resolved)) {
    if (dynamic_cast<IntType *>(p)) {
      if (auto in = dynamic_cast<IntType *>(init)) {
        if (in->bitWidth >= 32) {
          decl->resolved = in;
        }
        return;
      }
      Error::diagnostic(decl->span, "overflowed value");
    }

    if (dynamic_cast<FloatType *>(p)) {
      if (auto in = dynamic_cast<FloatType *>(init)) {
        if (in->bitWidth >= 32) {
          decl->resolved = in;
        }
        return;
      }

      if (auto in = dynamic_cast<IntType *>(init)) {
        if (in->bitWidth <= 24) {
          decl->resolved = table->getBuilt("f32");
          return;
        }
        if (in->bitWidth <= 53) {
          decl->resolved = table->getBuilt("f64");
          return;
        }
        if (in->bitWidth <= 113) {
          decl->resolved = table->getBuilt("f128");
          return;
        }

        Error::diagnostic(decl->span, "prection loss occured");
      }
    }

    if (dynamic_cast<CharType *>(p)) {
      if (auto in = dynamic_cast<CharType *>(init)) {
        decl->resolved = in;
        return;
      }
      Error::diagnostic(decl->span, "unmatched type");
    }

    if (dynamic_cast<StringType *>(p)) {
      if (auto in = dynamic_cast<StringType *>(init)) {
        decl->resolved = in;
        return;
      }
      Error::diagnostic(decl->span, "unmatched type");
    }
    if (dynamic_cast<BoolType *>(p)) {
      if (auto in = dynamic_cast<BoolType *>(init)) {
        decl->resolved = in;
        return;
      }
      Error::diagnostic(decl->span, "unmatched type");
    }
  }

  Error::diagnostic(decl->span, "unmatched type");
}

void Resolver::castFail(CastingFailKind kind, SourceSpan &span) {
  switch (kind) {

  case CastingFailKind::None:
    Error::diagnostic(span, "unknown kind of cast fail");
    break;

  case CastingFailKind::Overflow:
    Error::diagnostic(span, "overflow value");
    break;

  case CastingFailKind::Underflow:
    Error::diagnostic(span, "underfloat value");
    break;

  case CastingFailKind::SignToUnsign:
    Error::diagnostic(span, "casting sign to unsign");
    break;

  case CastingFailKind::NegativeToUnsigned:
    Error::diagnostic(span, "casting negative to unsign");
    break;

  case CastingFailKind::PrecisionLoss:
    Error::diagnostic(span, "pecicison loss occured");
    break;

  case CastingFailKind::FractionLoss:
    Error::diagnostic(span, "fraction loss occured");
    break;

  case CastingFailKind::Unmatched:
    Error::diagnostic(span, "unmatched type");
    break;

  case CastingFailKind::InvalidCategory:
    Error::diagnostic(span, "invaild type");
    break;

  case CastingFailKind::ExplicitRequired:
    Error::internal(span, "need explict cast");
    break;

  case CastingFailKind::Narrowing:
    Error::diagnostic(span, "unsafe narrowing occur");
    break;

  case CastingFailKind::NaN:
    Error::diagnostic(span, "not a number");
    break;

  case CastingFailKind::Infinity:
    Error::diagnostic(span, "infinity value");
    break;

  case CastingFailKind::NotImplemented:
    Error::diagnostic(span, "unknown kind of cast fail");
    break;
  }
}