#include "hrd/util/Helper.h"
#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/util/Error.h"
#include "hrd/util/TypeResolver.h"

std::string Helper::apIntToString(const llvm::APInt &value) {
  llvm::SmallString<32> buffer;
  value.toString(buffer, 10, false);
  return std::string(buffer.str());
}

pair<bool, SourceSpan>
Helper::hasSameMethodSig(const vector<MethodSymbol *> &methods,
                         MethodSymbol *symbol) {
  for (auto *method : methods) {
    if (method == symbol) {
      continue;
    }

    if (method->name != symbol->name) {
      continue;
    }

    if (method->returnType != symbol->returnType) {
      continue;
    }

    if (method->params.size() != symbol->params.size()) {
      continue;
    }

    bool same = true;
    SourceSpan span = method->decl->span;

    for (size_t i = 0; i < method->params.size(); ++i) {
      if (method->params[i]->typeSymbol != symbol->params[i]->typeSymbol) {
        same = false;
        break;
      }
    }

    if (same) {
      return {true, span};
    }
  }

  return {false, {}};
}

pair<bool, SourceSpan> Helper::hasSameSig(const vector<MethodSymbol *> &methods,
                                          MethodSymbol *symbol) {
  for (auto *method : methods) {
    if (method == symbol) {
      continue;
    }

    if (method->params.size() != symbol->params.size()) {
      continue;
    }

    bool same = true;
    SourceSpan span = method->decl->span;

    for (size_t i = 0; i < method->params.size(); ++i) {
      if (method->params[i]->typeSymbol != symbol->params[i]->typeSymbol) {
        same = false;
        break;
      }
    }

    if (same) {
      return {true, span};
    }
  }

  return {false, {}};
}

bool Helper::hasSameSig(const vector<TraitSig *> &signatures, TraitSig *sig) {
  for (auto *signature : signatures) {
    if (signature->params.size() != sig->params.size()) {
      continue;
    }

    bool same = true;

    for (size_t i = 0; i < signature->params.size(); ++i) {
      if (signature->params[i] != sig->params[i]) {
        same = false;
        break;
      }
    }

    if (same) {
      return true;
    }
  }

  return false;
}

void TypeResolver::resolveTypeNode(TypeNode *type, TypeResolverContext &ctx) {
  if (type == nullptr) {
    Error::internal("type node is nullptr");
  }

  if (dynamic_cast<BuiltinTypeNode *>(type) ||
      dynamic_cast<IdentifierTypeNode *>(type)) {
    auto *symbol = ctx.table.getType(type);

    if (symbol == nullptr) {
      auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S013);
      dia.labels = {
          {type->span, "type '" + type->type + "' is not declared", true},
      };
      dia.helps = {
          "declare the type before using it",
      };
      ctx.engine.emit(dia);
      ctx.recover.recover();
    }

    type->resolved = symbol;
    return;
  }

  if (auto *array = dynamic_cast<ArrayTypeNode *>(type)) {
    if (array->elementType == nullptr) {
      Error::internal(array->span, "array element type node is nullptr");
    }

    if (array->fixedSize == nullptr) {
      Error::internal(array->span, "fixed array size expression is nullptr");
    }

    resolveTypeNode(array->elementType.get(), ctx);

    if (array->elementType->resolved == nullptr) {
      Error::internal(array->elementType->span,
                      "array element type was not resolved");
    }

    llvm::APInt size = resolveFixedArraySize(array->fixedSize.get(), ctx);

    array->resolved =
        ctx.table.registry.getOrCreateArray(array->elementType->resolved, size);

    if (array->resolved == nullptr) {
      Error::internal(array->span, "failed to create array type");
    }

    return;
  }

  if (auto *generic = dynamic_cast<GenericTypeNode *>(type)) {
    vector<TypeSymbol *> args;
    args.reserve(generic->typeArgs.size());

    for (auto &arg : generic->typeArgs) {
      if (arg == nullptr) {
        Error::internal(generic->span, "generic type argument is nullptr");
      }

      resolveTypeNode(arg.get(), ctx);

      if (arg->resolved == nullptr) {
        Error::internal(arg->span,
                        "generic type argument was not resolved: " + arg->type);
      }

      args.push_back(arg->resolved);
    }

    TypeSymbol *origin = nullptr;

    switch (generic->gKind) {
    case GenericTypeNode::GenericKind::HANDLE: {
      if (args.size() != 1) {
        auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S109);
        dia.labels = {
            {generic->span,
             "Handle has " + to_string(args.size()) + " type arguments", true},
        };
        dia.notes = {
            "Handle requires exactly one type argument",
        };
        dia.helps = {
            "use 'Handle<T>' with one entity type",
        };
        ctx.engine.emit(dia);
        ctx.recover.recover();
      }

      auto *target = args[0];

      if (target->kind != TypeSymbol::TypeKind::CLASS ||
          target->type == Symbol::SymbolType::MAIN) {
        auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S110);
        dia.labels = {
            {generic->typeArgs[0]->span,
             "type '" + target->name + "' cannot be used as a Handle target",
             true},
        };
        dia.notes = {
            "Handle can only reference non-Main class entity types",
        };
        dia.helps = {
            "use a class entity type as the Handle target",
        };
        ctx.engine.emit(dia);
        ctx.recover.recover();
      }

      origin = ctx.table.registry.getHandle();
      break;
    }

    case GenericTypeNode::GenericKind::OPTION: {
      if (args.size() != 1) {
        auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S111);
        dia.labels = {
            {generic->span,
             "Option has " + to_string(args.size()) + " type arguments", true},
        };
        dia.notes = {
            "Option requires exactly one type argument",
        };
        dia.helps = {
            "use 'Option<T>' with one type",
        };
        ctx.engine.emit(dia);
        ctx.recover.recover();
      }

      origin = ctx.table.registry.getOption();
      break;
    }

    case GenericTypeNode::GenericKind::RESULT: {
      if (args.size() != 2) {
        auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S112);
        dia.labels = {
            {generic->span,
             "Result has " + to_string(args.size()) + " type arguments", true},
        };
        dia.notes = {
            "Result requires a value type and an error type",
        };
        dia.helps = {
            "use 'Result<T, E>' with exactly two type arguments",
        };
        ctx.engine.emit(dia);
        ctx.recover.recover();
      }

      if (args[1]->kind != TypeSymbol::TypeKind::ERROR) {
        auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S113);
        dia.labels = {
            {generic->typeArgs[1]->span,
             "type '" + args[1]->name + "' is not an error type", true},
        };
        dia.notes = {
            "the second type argument of Result must be an error type",
        };
        dia.helps = {
            "use an error type as the second Result argument",
        };
        ctx.engine.emit(dia);
        ctx.recover.recover();
      }

      origin = ctx.table.registry.getResult();
      break;
    }
    }

    if (origin == nullptr) {
      Error::internal(generic->span, "generic origin type is nullptr");
    }

    generic->resolved = ctx.table.registry.getOrCreateGeneric(origin, args);

    if (generic->resolved == nullptr) {
      Error::internal(generic->span, "failed to create generic type instance");
    }

    return;
  }

  auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S013);
  dia.labels = {
      {type->span, "this type syntax cannot be resolved", true},
  };
  dia.helps = {
      "use a builtin, declared, array, or supported generic type",
  };
  ctx.engine.emit(dia);
  ctx.recover.recover();
}

llvm::APInt TypeResolver::resolveFixedArraySize(Expr *expr,
                                                TypeResolverContext &ctx) {
  if (expr == nullptr) {
    Error::internal("fixed array size expression is nullptr");
  }

  auto *literal = dynamic_cast<LiteralExpr *>(expr);

  if (literal == nullptr || literal->token.kind != TKind::LIT_INT) {
    auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S114);
    dia.labels = {
        {expr->span, "this expression is not an integer literal", true},
    };
    dia.notes = {
        "a fixed array size must be known at compile time",
    };
    dia.helps = {
        "use a positive integer literal as the array size",
    };
    ctx.engine.emit(dia);
    ctx.recover.recover();
  }

  ResolvedLit resolved = resolveLitInt(literal, ctx);

  literal->resolvedType = resolved.type;
  literal->resolvedLit = resolved;

  if (!literal->resolvedLit.isInt()) {
    Error::internal(literal->span,
                    "fixed array size literal did not resolve as integer");
  }

  llvm::APInt value = literal->resolvedLit.asInt().value;

  if (value.isNegative()) {
    auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S115);
    dia.labels = {
        {expr->span, "array size is negative", true},
    };
    dia.notes = {
        "a fixed array size cannot be negative",
    };
    dia.helps = {
        "use a positive integer literal",
    };
    ctx.engine.emit(dia);
    ctx.recover.recover();
  }

  if (value.isZero()) {
    auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S116);
    dia.labels = {
        {expr->span, "array size is zero", true},
    };
    dia.notes = {
        "a fixed array must contain at least one element",
    };
    dia.helps = {
        "use an integer literal greater than zero",
    };
    ctx.engine.emit(dia);
    ctx.recover.recover();
  }

  return value.zextOrTrunc(64);
}

static int compareUnsignedDecimal(const std::string &left,
                                  const std::string &right) {
  if (left.size() < right.size()) {
    return -1;
  }

  if (left.size() > right.size()) {
    return 1;
  }

  if (left < right) {
    return -1;
  }

  if (left > right) {
    return 1;
  }

  return 0;
}

ResolvedLit TypeResolver::resolveLitInt(LiteralExpr *expr,
                                        TypeResolverContext &ctx) {
  if (expr == nullptr) {
    Error::internal("integer literal expression is nullptr");
  }

  const string max32 = "2147483647";
  const string max64 = "9223372036854775807";
  const string max128 = "170141183460469231731687303715884105727";

  std::string value = expr->value;

  const size_t position = value.find_first_not_of('0');

  if (position == std::string::npos) {
    value = "0";
  } else {
    value = value.substr(position);
  }

  unsigned bits = 0;

  if (compareUnsignedDecimal(value, max32) <= 0) {
    bits = 32;
  } else if (compareUnsignedDecimal(value, max64) <= 0) {
    bits = 64;
  } else if (compareUnsignedDecimal(value, max128) <= 0) {
    bits = 128;
  } else {
    auto dia = ctx.engine.makeDiagnostic(DiagnosticCode::HRD_S117);
    dia.labels = {
        {expr->token.span,
         "this integer literal exceeds the supported i128 range", true},
    };
    dia.notes = {
        "i128 is the largest supported signed integer type",
    };
    dia.helps = {
        "reduce the integer literal value",
    };
    ctx.engine.emit(dia);
    ctx.recover.recover();
  }

  ResolvedLit resolved;

  switch (bits) {
  case 32:
    resolved.type = ctx.table.getType("i32");
    break;

  case 64:
    resolved.type = ctx.table.getType("i64");
    break;

  case 128:
    resolved.type = ctx.table.getType("i128");
    break;

  default:
    Error::internal(expr->token, "invalid integer literal bit width");
  }

  if (resolved.type == nullptr) {
    Error::internal(expr->span, "failed to find inferred integer literal type");
  }

  resolved.value = IntPayload(llvm::APInt(bits, llvm::StringRef(value), 10));

  return resolved;
}

pair<bool, TypeSymbol *> Helper::checkImplementTraitSig(TypeSymbol *symbol) {
  if (symbol == nullptr) {
    Error::internal("type symbol is nullptr");
  }

  for (auto *traitType : symbol->traits) {
    if (traitType == nullptr || traitType->decl == nullptr) {
      Error::internal("invalid trait type");
    }

    auto *trait = dynamic_cast<TraitDecl *>(traitType->decl);

    if (trait == nullptr) {
      Error::internal("trait type declaration is not TraitDecl");
    }

    for (auto &sig : trait->traitSigs) {
      if (sig == nullptr || sig->symbol == nullptr) {
        Error::internal("invalid trait signature");
      }

      if (!Helper::hasMethodInHierarchyWithSameSig(symbol, sig->name,
                                                   sig->symbol)) {
        return {false, traitType};
      }
    }
  }

  return {true, nullptr};
}

bool Helper::hasMethodInHierarchyWithSameSig(TypeSymbol *type,
                                             const std::string &name,
                                             MethodSymbol *sig) {
  for (auto *current = type; current != nullptr; current = current->base) {
    if (current->memberScope == nullptr) {
      continue;
    }

    auto it = current->memberScope->methodMap.find(name);

    if (it == current->memberScope->methodMap.end()) {
      continue;
    }

    if (Helper::hasSameSig(it->second, sig).first) {
      return true;
    }
  }

  return false;
}

pair<bool, CastingResultKind> Helper::canImplicitlyConvert(TypeSymbol *from,
                                                           TypeSymbol *to) {
  if (!from) {
    Error::internal("source type is nullptr");
  }

  if (!to) {
    Error::internal("target type is nullptr");
  }

  for (auto *type = from; type != nullptr; type = type->base) {
    if (type == to) {
      return {true, CastingResultKind::None};
    }
  }

  if (from->kind != TypeSymbol::TypeKind::PRIMITIVE ||
      to->kind != TypeSymbol::TypeKind::PRIMITIVE) {
    return {false, CastingResultKind::Unmatched};
  }

  auto *source = static_cast<PrimtiveType *>(from);
  auto *target = static_cast<PrimtiveType *>(to);

  if (isa<IntType>(source) && isa<IntType>(target)) {
    auto *sourceInt = static_cast<IntType *>(source);
    auto *targetInt = static_cast<IntType *>(target);

    if (sourceInt->isSigned == targetInt->isSigned) {
      return {targetInt->bitWidth >= sourceInt->bitWidth,
              CastingResultKind::Overflow};
    }

    if (sourceInt->isSigned && !targetInt->isSigned) {
      return {false, CastingResultKind::SignToUnsign};
    }

    if (!sourceInt->isSigned && targetInt->isSigned) {
      return {targetInt->bitWidth >= sourceInt->bitWidth,
              CastingResultKind::Overflow};
    }

    return {targetInt->bitWidth >= sourceInt->bitWidth,
            CastingResultKind::Overflow};
  }

  if (isa<FloatType>(source) && isa<FloatType>(target)) {
    return {static_cast<FloatType *>(target)->bitWidth >=
                static_cast<FloatType *>(source)->bitWidth,
            CastingResultKind::Overflow};
  }

  if (isa<IntType>(source) && isa<FloatType>(target)) {
    auto *sourceInt = static_cast<IntType *>(source);
    auto *targetFloat = static_cast<FloatType *>(target);

    if (sourceInt->bitWidth == targetFloat->bitWidth) {
      return {true, CastingResultKind::PrecisionLoss};
    }

    if (sourceInt->bitWidth <= targetFloat->precious) {
      return {true, CastingResultKind::None};
    }

    return {false, CastingResultKind::Overflow};
  }

  return {false, CastingResultKind::Unmatched};
}