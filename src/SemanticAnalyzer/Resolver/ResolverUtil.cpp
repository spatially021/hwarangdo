#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Expr.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/SymbolHelper.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include "hrd/util/Helper.h"
#include <cassert>
#include <unordered_set>

ValueSymbol *Resolver::resolveValue(str name) {
  if (auto *value = table.scopeManger.getValue(name)) {
    return value;
  }

  if (currentSelf) {
    auto it = currentSelf->value.find(name);
    if (it != currentSelf->value.end()) {
      return it->second.get();
    }
  }

  return nullptr;
}

ValueSymbol *Resolver::lookLocalValue(str name, Scope *localScope) {
  auto it = localScope->value.find(name);
  if (it != localScope->value.end()) {
    return it->second.get();
  }

  return nullptr;
}

bool Resolver::isAssignable(TypeSymbol *from, TypeSymbol *to) {
  return Helper::canImplicitlyConvert(from, to).first;
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
    return isa<IntType>(left) && isa<IntType>(right);

  case Operator::ADD:
    if (isa<StringType>(left) && isa<StringType>(right)) {
      return true;
    }

    return SymbolHelper::isNumberic(left) && SymbolHelper::isNumberic(right);

  case Operator::LS:
  case Operator::LSE:
  case Operator::GR:
  case Operator::GRE:
  case Operator::SUB:
  case Operator::MUL:
  case Operator::DIV:
  case Operator::REM:
  case Operator::POW:
    return SymbolHelper::isNumberic(left) && SymbolHelper::isNumberic(right);

  case Operator::AND:
  case Operator::OR:
    return isa<BoolType>(left) && isa<BoolType>(right);

  case Operator::EQ:
  case Operator::NT:
    return isCmpable(left, right);

  case Operator::L_NOT:
  case Operator::B_NOT:
  case Operator::PLUS:
  case Operator::MINUS:
    Error::internal("unmatched operator type");
  }

  return false;
}

bool Resolver::isCmpable(TypeSymbol *left, TypeSymbol *right) {
  if (left->kind == TypeKind::CLASS || right->kind == TypeKind::CLASS) {
    return false;
  }

  if (SymbolHelper::isNumberic(left) && SymbolHelper::isNumberic(right)) {
    return true;
  }

  if (isa<BoolType>(left) && isa<BoolType>(right)) {
    return true;
  }

  if (left->kind == TypeKind::ENUM && right->kind == TypeKind::ENUM) {
    return left == right;
  }

  return left == right;
}

pair<TypeSymbol *, CastingResultKind>
Resolver::binaryResult(Operator op, TypeSymbol *left, TypeSymbol *right) {
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
  case Operator::POW: {
    auto result = binaryCasting(left, right);
    if (result.first) {
      return result;
    }

    Error::internal("failed to determine binary operand type");
  }

  case Operator::LS:
  case Operator::LSE:
  case Operator::GR:
  case Operator::GRE:
  case Operator::AND:
  case Operator::OR:
  case Operator::EQ:
  case Operator::NT:
    return {table.registry.getBuilt("bool"), CastingResultKind::None};

  case Operator::L_NOT:
  case Operator::B_NOT:
  case Operator::PLUS:
  case Operator::MINUS:
    Error::internal("unmatched operator type");
  }

  return {nullptr, CastingResultKind::Unmatched};
}

[[noreturn]] void Resolver::unmatchSymbol(Symbol *symbol) {
  Error::internal(symbol->name + ": unmatched symbol");
}

bool Resolver::isCastable(TypeSymbol *from, TypeSymbol *to) {
  return from == to;
}

pair<TypeSymbol *, CastingResultKind>
Resolver::binaryCasting(TypeSymbol *left, TypeSymbol *right) {
  if (left == right) {
    return {left, CastingResultKind::None};
  }

  vector<TypeSymbol *> candidates = getPromotionCandidates(left, right);

  for (auto *candidate : candidates) {
    auto leftResult = Helper::canImplicitlyConvert(left, candidate);
    auto rightResult = Helper::canImplicitlyConvert(right, candidate);

    if (leftResult.first && rightResult.first) {
      if (leftResult.second == CastingResultKind::PrecisionLoss) {
        return {candidate, leftResult.second};
      }

      return {candidate, rightResult.second};
    }
  }

  return {nullptr, CastingResultKind::Unmatched};
}

vector<TypeSymbol *> Resolver::getPromotionCandidates(TypeSymbol *left,
                                                      TypeSymbol *right) {
  vector<TypeSymbol *> result;

  if (left->kind != TypeKind::PRIMITIVE && right->kind != TypeKind::PRIMITIVE) {
    return result;
  }

  if (left->kind != TypeKind::PRIMITIVE || right->kind != TypeKind::PRIMITIVE) {
    unordered_set<TypeSymbol *> used;
    if (auto obj = dyn_cast<ObjectType>(left)) {
      for (auto *type = obj; type != nullptr; type = type->base) {
        auto it = used.find(type);
        if (it == used.end()) {
          result.push_back(type);
          used.emplace(type);
        }
      }
    }

    if (auto obj = dyn_cast<ObjectType>(right)) {
      for (auto *type = obj; type != nullptr; type = type->base) {
        auto it = used.find(type);
        if (it == used.end()) {
          result.push_back(type);
          used.emplace(type);
        }
      }
    }

    return result;
  }

  auto *leftPrimitive = static_cast<PrimtiveType *>(left);
  auto *rightPrimitive = static_cast<PrimtiveType *>(right);

  if (isa<FloatType>(leftPrimitive) || isa<FloatType>(rightPrimitive)) {
    if (isa<FloatType>(leftPrimitive)) {
      result.push_back(left);
    }

    if (isa<FloatType>(rightPrimitive)) {
      result.push_back(right);
    }

    return result;
  }

  if (isa<IntType>(leftPrimitive) && isa<IntType>(rightPrimitive)) {
    auto *bigger = static_cast<IntType *>(leftPrimitive)->bitWidth >=
                           static_cast<IntType *>(rightPrimitive)->bitWidth
                       ? left
                       : right;

    auto *smaller = bigger == left ? right : left;

    result.push_back(bigger);
    result.push_back(smaller);
    return result;
  }

  if (isa<StringType>(leftPrimitive) && isa<StringType>(rightPrimitive)) {
    auto *bigger = static_cast<StringType *>(leftPrimitive)->bitWidth >=
                           static_cast<StringType *>(rightPrimitive)->bitWidth
                       ? left
                       : right;

    auto *smaller = bigger == left ? right : left;

    result.push_back(bigger);
    result.push_back(smaller);
    return result;
  }

  return result;
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
    Error::internal("illegal float bit width");
  }
}

pair<bool, CastingResultKind>
Resolver::canImplicitlyLiteralConvert(LiteralExpr *from, TypeSymbol *to) {
  assert(from);
  assert(to);

  if (from->resolvedType == to) {
    return {true, CastingResultKind::None};
  }

  if (to->kind != TypeKind::PRIMITIVE) {
    return {false, CastingResultKind::Unmatched};
  }

  auto *target = static_cast<PrimtiveType *>(to);

  switch (from->token.kind) {
  case TKind::LIT_INT: {
    if (!isa<IntType>(target) && !isa<FloatType>(target)) {
      return {false, CastingResultKind::InvalidCategory};
    }

    const auto &value = from->resolvedLit.asInt().value;

    if (isa<IntType>(target)) {
      auto *targetInt = static_cast<IntType *>(target);

      if (targetInt->isSigned) {
        return {value.isSignedIntN(targetInt->bitWidth),
                CastingResultKind::Overflow};
      }

      if (value.isNegative()) {
        return {false, CastingResultKind::NegativeToUnsigned};
      }

      return {value.getActiveBits() <= targetInt->bitWidth,
              CastingResultKind::Overflow};
    }

    if (isa<FloatType>(target)) {
      auto *targetFloat = static_cast<FloatType *>(target);

      return {value.getSignificantBits() <= targetFloat->precious,
              CastingResultKind::PrecisionLoss};
    }

    return {false, CastingResultKind::Unmatched};
  }

  case TKind::LIT_FLOAT: {
    if (!isa<FloatType>(target)) {
      return {false, CastingResultKind::Unmatched};
    }

    auto *targetFloat = static_cast<FloatType *>(target);
    const auto &value = from->resolvedLit.asFloat().value;

    llvm::APFloat converted = value;
    bool losesInfo = false;

    converted.convert(getFloatSemantics(targetFloat),
                      llvm::APFloat::rmNearestTiesToEven, &losesInfo);

    return {!losesInfo, CastingResultKind::FractionLoss};
  }

  case TKind::LIT_CHARACTER: {
    if (!isa<CharType>(target)) {
      return {false, CastingResultKind::Unmatched};
    }

    auto *targetChar = static_cast<CharType *>(target);
    auto codePoint = from->resolvedLit.asChar().codePoint;

    switch (targetChar->bitWidth) {
    case 8:
      return {codePoint <= 0x7F, CastingResultKind::Overflow};

    case 16:
      return {codePoint <= 0xFFFF, CastingResultKind::Overflow};

    case 32:
      return {codePoint <= 0x10FFFF, CastingResultKind::Overflow};

    default:
      return {false, CastingResultKind::Overflow};
    }
  }

  case TKind::LIT_STRING: {
    if (!isa<StringType>(target)) {
      return {false, CastingResultKind::Unmatched};
    }

    auto *targetString = static_cast<StringType *>(target);

    for (auto codePoint : from->resolvedLit.asString().codePoints) {
      switch (targetString->bitWidth) {
      case 8:
        if (codePoint > 0x7F) {
          return {false, CastingResultKind::Overflow};
        }
        break;

      case 16:
        if (codePoint > 0xFFFF) {
          return {false, CastingResultKind::Overflow};
        }
        break;

      case 32:
        if (codePoint > 0x10FFFF) {
          return {false, CastingResultKind::Overflow};
        }
        break;

      default:
        return {false, CastingResultKind::Overflow};
      }
    }

    return {true, CastingResultKind::None};
  }

  case TKind::LIT_BOOL:
    return {to == table.registry.getBool(), CastingResultKind::Unmatched};

  default:
    return {false, CastingResultKind::NotImplemented};
  }
}

pair<TypeSymbol *, CastingResultKind>
Resolver::implicitCasting(Expr *from, TypeSymbol *to) {
  if (auto *literal = dynamic_cast<LiteralExpr *>(from)) {
    auto [result, kind] = canImplicitlyLiteralConvert(literal, to);

    if (result) {
      return {to, kind};
    }

    return {nullptr, kind};
  }

  auto [result, kind] = Helper::canImplicitlyConvert(from->resolvedType, to);

  if (result) {
    return {to, kind};
  }

  return {nullptr, kind};
}

ValueSymbol *Resolver::lookupEnumVariant(EnumType *enumType, const string &name,
                                         SourceSpan &token) {

  auto it = enumType->variantMap.find(name);
  if (it == enumType->variantMap.end()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S028);
    dia.labels = {
        {token,
         "enum '" + enumType->name + "' has no variant named '" + name + "'",
         true},
    };
    dia.notes = {
        "the variant must be declared by the referenced enum type",
    };
    dia.helps = {
        "use a variant declared by '" + enumType->name + "'",
    };
    engine.emit(dia);
    recover.recover();
  }

  return it->second;
}

// pair<bool, MethodSymbol *> Resolver::lookupMethod(str name, Scope *scope,
//                                                   vector<TypeSymbol *> args)
//                                                   {
//   auto &bucket = scope->methodMap[name];

//   if (bucket.empty() && args.empty()) {
//     return {true, nullptr};
//   }

//   for (auto &method : bucket) {
//     if (method->params.size() != args.size()) {
//       continue;
//     }

//     bool matches = true;

//     for (unsigned int i = 0; i < args.size(); ++i) {
//       if (method->params[i]->typeSymbol != args[i]) {
//         matches = false;
//         break;
//       }
//     }

//     if (matches) {
//       return {true, method};
//     }
//   }

//   return {false, nullptr};
// }

pair<bool, MethodSymbol *> Resolver::lookupInit(ObjectType *type,
                                                vector<TypeSymbol *> args) {
  auto &bucket = type->inits;

  if (bucket.empty() && args.empty()) {
    return {true, nullptr};
  }

  for (auto &method : bucket) {
    if (method->params.size() != args.size()) {
      continue;
    }

    bool matches = true;

    for (unsigned int i = 0; i < args.size(); ++i) {
      if (method->params[i]->typeSymbol != args[i]) {
        matches = false;
        break;
      }
    }

    if (matches) {
      return {true, method};
    }
  }

  return {false, nullptr};
}

static unsigned minSignedBits(const llvm::APInt &value) {
  if (value.isNegative()) {
    return (~value).getActiveBits() + 1;
  }

  return value.getActiveBits() + 1;
}

void Resolver::convertLit(LiteralExpr *lit, TypeNode *type) {
  auto *primitive = dynamic_cast<PrimtiveType *>(type->resolved);

  if (primitive == nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S089);
    dia.labels = {
        {type->span, "this type does not support primitive literal inference",
         true},
        {lit->span, "literal used to initialize this type", false},
    };
    dia.notes = {
        "literal type inference is only available for primitive types",
    };
    dia.helps = {
        "use an explicit value compatible with the declared type",
    };
    engine.emit(dia);
    recover.recover();
  }

  switch (primitive->builtinCategory) {
  case BuiltinCategory::Float:
    if (lit->resolvedLit.isInt()) {
      const llvm::APInt &value = lit->resolvedLit.asInt().value;
      auto requiredBits = minSignedBits(value);

      if (requiredBits <= 24) {
        type->resolved = table.getType("f32");
        return;
      }

      if (requiredBits <= 53) {
        type->resolved = table.getType("f64");
        return;
      }

      if (requiredBits <= 113) {
        type->resolved = table.getType("f128");
        return;
      }

      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S090);
      dia.labels = {
          {lit->span,
           "this integer literal cannot be represented by any float type",
           true},
      };
      dia.notes = {
          "the largest supported floating-point type provides 113 bits of "
          "integer precision",
      };
      dia.helps = {
          "use an integer type or reduce the literal value",
      };
      engine.emit(dia);
      recover.recover();
    }
    break;

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
  auto *declaredPrimitive = dynamic_cast<PrimtiveType *>(decl->resolved);

  if (!declaredPrimitive) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
    dia.labels = {
        {decl->span, "this declaration does not have a primitive type", true},
    };
    dia.notes = {
        "primitive type inference requires a primitive declared type",
    };
    dia.helps = {
        "use an explicit initializer compatible with the declared type",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (dynamic_cast<IntType *>(declaredPrimitive)) {
    if (auto *initialType = dynamic_cast<IntType *>(init)) {
      if (initialType->bitWidth >= 32) {
        decl->resolved = initialType;
      }

      return;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
    dia.labels = {
        {decl->span,
         "integer type cannot be inferred from type '" + init->name + "'",
         true},
    };
    dia.notes = {
        "an integer declaration requires an integer initializer",
    };
    dia.helps = {
        "use an integer initializer",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (dynamic_cast<FloatType *>(declaredPrimitive)) {
    if (auto *initialType = dynamic_cast<FloatType *>(init)) {
      if (initialType->bitWidth >= 32) {
        decl->resolved = initialType;
      }

      return;
    }

    if (auto *initialType = dynamic_cast<IntType *>(init)) {
      if (initialType->bitWidth <= 24) {
        decl->resolved = table.registry.getBuilt("f32");
        return;
      }

      if (initialType->bitWidth <= 53) {
        decl->resolved = table.registry.getBuilt("f64");
        return;
      }

      if (initialType->bitWidth <= 113) {
        decl->resolved = table.registry.getBuilt("f128");
        return;
      }

      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S096);
      dia.labels = {
          {decl->span,
           "integer type '" + initialType->name +
               "' cannot be represented exactly by any float type",
           true},
      };
      dia.notes = {
          "implicit integer-to-float conversion is only allowed when the value "
          "can be represented exactly",
      };
      dia.helps = {
          "use an explicit cast or keep the value as an integer",
      };
      engine.emit(dia);
      recover.recover();
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
    dia.labels = {
        {decl->span,
         "floating-point type cannot be inferred from type '" + init->name +
             "'",
         true},
    };
    dia.notes = {
        "a floating-point declaration requires an integer or floating-point "
        "initializer",
    };
    dia.helps = {
        "use a numeric initializer",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (dynamic_cast<CharType *>(declaredPrimitive)) {
    if (auto *initialType = dynamic_cast<CharType *>(init)) {
      decl->resolved = initialType;
      return;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
    dia.labels = {
        {decl->span,
         "character type cannot be inferred from type '" + init->name + "'",
         true},
    };
    dia.notes = {
        "a character declaration requires a character initializer",
    };
    dia.helps = {
        "use a character initializer",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (dynamic_cast<StringType *>(declaredPrimitive)) {
    if (auto *initialType = dynamic_cast<StringType *>(init)) {
      decl->resolved = initialType;
      return;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
    dia.labels = {
        {decl->span,
         "string type cannot be inferred from type '" + init->name + "'", true},
    };
    dia.notes = {
        "a string declaration requires a string initializer",
    };
    dia.helps = {
        "use a string initializer",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (dynamic_cast<BoolType *>(declaredPrimitive)) {
    if (auto *initialType = dynamic_cast<BoolType *>(init)) {
      decl->resolved = initialType;
      return;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
    dia.labels = {
        {decl->span,
         "boolean type cannot be inferred from type '" + init->name + "'",
         true},
    };
    dia.notes = {
        "a boolean declaration requires a boolean initializer",
    };
    dia.helps = {
        "use a boolean initializer",
    };
    engine.emit(dia);
    recover.recover();
  }

  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S091);
  dia.labels = {
      {decl->span, "this primitive type does not support type inference", true},
  };
  dia.helps = {
      "specify a concrete primitive type",
  };
  engine.emit(dia);
  recover.recover();
}

void Resolver::castFail(CastingResultKind kind, SourceSpan &span) {
  switch (kind) {
  case CastingResultKind::None:
    Error::internal(span, "successful conversion reported as a cast failure");

  case CastingResultKind::Overflow: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S092);
    dia.labels = {
        {span, "the value is outside the target type's representable range",
         true},
    };
    dia.notes = {
        "this conversion would overflow the target type",
    };
    dia.helps = {
        "use a wider target type or reduce the value",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::Underflow: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S093);
    dia.labels = {
        {span, "the value is too small for the target type", true},
    };
    dia.notes = {
        "this conversion would underflow the target type",
    };
    dia.helps = {
        "use a type with a wider representable range",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::SignToUnsign: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S094);
    dia.labels = {
        {span, "signed value cannot be implicitly converted to unsigned", true},
    };
    dia.notes = {
        "a signed value may contain a negative value that an unsigned type "
        "cannot represent",
    };
    dia.helps = {
        "use a signed target type or perform an explicit cast",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::NegativeToUnsigned: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S095);
    dia.labels = {
        {span, "negative value cannot be represented by an unsigned type",
         true},
    };
    dia.helps = {
        "use a signed target type or a non-negative value",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::PrecisionLoss: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S096);
    dia.labels = {
        {span, "this conversion cannot preserve the value exactly", true},
    };
    dia.notes = {
        "the target type does not provide enough precision",
    };
    dia.helps = {
        "use a more precise target type or perform an explicit cast",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::FractionLoss: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S097);
    dia.labels = {
        {span, "this conversion changes the fractional value", true},
    };
    dia.notes = {
        "the target floating-point type cannot represent the source value "
        "exactly",
    };
    dia.helps = {
        "use a wider floating-point type or perform an explicit cast",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::Unmatched: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S098);
    dia.labels = {
        {span, "no conversion exists between these types", true},
    };
    dia.helps = {
        "use a value compatible with the target type",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::InvalidCategory: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S098);
    dia.labels = {
        {span, "this value category cannot be converted to the target type",
         true},
    };
    dia.notes = {
        "implicit conversion is only supported between compatible type "
        "categories",
    };
    dia.helps = {
        "use a target type compatible with this value",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::ExplicitRequired: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S099);
    dia.labels = {
        {span, "this conversion cannot be performed implicitly", true},
    };
    dia.notes = {
        "the conversion is available only when explicitly requested",
    };
    dia.helps = {
        "add an explicit cast to the target type",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::Narrowing: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S100);
    dia.labels = {
        {span, "the target type is narrower than the source type", true},
    };
    dia.notes = {
        "this conversion may discard part of the source value",
    };
    dia.helps = {
        "use a wider target type or perform an explicit cast",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::NaN: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S101);
    dia.labels = {
        {span, "this value is not a valid number", true},
    };
    dia.notes = {
        "the target conversion does not accept NaN",
    };
    dia.helps = {
        "use a finite numeric value",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::Infinity: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S102);
    dia.labels = {
        {span, "infinite value cannot be represented by the target type", true},
    };
    dia.helps = {
        "use a finite value within the target type's range",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case CastingResultKind::NotImplemented: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S103);
    dia.labels = {
        {span, "this conversion is not currently supported", true},
    };
    dia.notes = {
        "the source and target types use a conversion that has not been "
        "implemented",
    };
    dia.helps = {
        "use a directly compatible value or an available conversion",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }
  }

  Error::internal(span, "unhandled casting result kind");
}