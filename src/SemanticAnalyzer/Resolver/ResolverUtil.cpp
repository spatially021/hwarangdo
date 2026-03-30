#include "AST/Expr.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
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
  return canImplicitlyConvert(from, to);
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
    if (auto temp = binaryCasting(left, right)) {
      return temp;
    }
    Error::internal("fail to binary casting");

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
    if (canImplicitlyConvert(left, T) && canImplicitlyConvert(right, T))
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

bool Resolver::canImplicitlyConvert(TypeSymbol *from, TypeSymbol *to) {

  if (!from) {
    Error::symbol(*from, "from is nullptr");
  }

  if (!to) {
    Error::symbol(*to, "to is nullptr");
  }

  if (from == to)
    return true;
  if (from->kind != TypeSymbol::TypeKind::PRIMITIVE ||
      to->kind != TypeSymbol::TypeKind::PRIMITIVE) {
    return false;
  }

  auto f = static_cast<PrimtiveType *>(from);
  auto t = static_cast<PrimtiveType *>(to);

  if (table->isInt(f) && table->isInt(t)) {
    auto fi = static_cast<IntType *>(f);
    auto ti = static_cast<IntType *>(t);

    if (fi->isSigned == ti->isSigned)
      return ti->bitWidth >= fi->bitWidth;

    // signed → unsigned : 금지
    if (fi->isSigned && !ti->isSigned)
      return false;

    // unsigned → signed
    if (!fi->isSigned && ti->isSigned)
      return ti->bitWidth > fi->bitWidth;

    return false;
  }

  if (table->isFloat(f) && table->isFloat(t)) {
    return static_cast<FloatType *>(t)->bitWidth >=
           static_cast<FloatType *>(f)->bitWidth;
  }

  if (table->isInt(f) && table->isFloat(t)) {

    auto fi = static_cast<IntType *>(f);
    auto tf = static_cast<FloatType *>(t);

    return fi->bitWidth <= tf->precious;
  }

  return false;
}

TypeSymbol *Resolver::implicitCasting(TypeSymbol *from, TypeSymbol *to) {
  if (canImplicitlyConvert(from, to)) {
    return to;
  }
  return nullptr;
}

ValueSymbol *Resolver::lookupEnumVariant(TypeSymbol *enumType,
                                         const string &name, Token token) {
  if (!enumType || enumType->kind != TypeSymbol::TypeKind::ENUM) {
    Error::internal(token, "expected enum type");
  }

  auto it = enumType->variantMap.find(name);
  if (it == enumType->variantMap.end()) {
    Error::diagnostic(token, name + " is not " + enumType->name + "'s variant");
  }

  return it->second;
}