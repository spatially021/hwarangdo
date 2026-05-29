#include "util/Helper.h"
#include "AST/ASTNode.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "util/Error.h"
#include "util/TypeResolver.h"
std::string Helper::apIntToString(const llvm::APInt &v) {
  llvm::SmallString<32> buf;
  v.toString(buf, 10, false);
  return std::string(buf.str());
}

bool Helper::hasSameMethodSig(const vector<MethodSymbol *> &vec,
                              MethodSymbol *symbol) {
  for (auto *m : vec) {
    if (m == symbol)
      continue;
    if (m->name != symbol->name)
      continue;
    if (m->returnType != symbol->returnType) {
      continue;
    }

    if (m->paramTypes.size() != symbol->paramTypes.size()) {
      continue;
    }

    bool same = true;
    for (size_t i = 0; i < m->paramTypes.size(); ++i) {
      if (m->paramTypes[i] != symbol->paramTypes[i]) {
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

bool Helper::hasSameSig(const vector<MethodSymbol *> &vec,
                        MethodSymbol *symbol) {
  for (auto *m : vec) {
    if (m == symbol)
      continue;

    if (m->paramTypes.size() != symbol->paramTypes.size()) {
      continue;
    }

    bool same = true;
    for (size_t i = 0; i < m->paramTypes.size(); ++i) {
      if (m->paramTypes[i] != symbol->paramTypes[i]) {
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

bool Helper::hasSameSig(const vector<TraitSig *> &vec, TraitSig *sig) {
  for (auto *m : vec) {
    if (m->params.size() != sig->params.size()) {
      continue;
    }

    bool same = true;
    for (size_t i = 0; i < m->params.size(); ++i) {
      if (m->params[i] != sig->params[i]) {
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

void TypeResolver::resolveTypeNode(TypeNode *type, SymbolTable *table) {
  if (dynamic_cast<BuiltinTypeNode *>(type) ||
      dynamic_cast<IdentifierTypeNode *>(type)) {
    auto symbol = table->getType(type);
    if (!symbol)
      Error::diagnostic(type->span, "unknown type : " + type->type);
    type->resolved = symbol;
  } else if (auto a = dynamic_cast<ArrayTypeNode *>(type)) {
    TypeResolver::resolveTypeNode(a->elementType.get(), table);
    llvm::APInt size = resolveFixedArraySize(a->fixedSize.get(), table);
    a->resolved = table->arrayTypeGetOrCreate(a->elementType->resolved, size);
  } else if (auto g = dynamic_cast<GenericTypeNode *>(type)) {
    auto ar = g->typeArgs;
    TypeSymbol *orign = nullptr;
    vector<TypeSymbol *> args;
    for (auto &t : g->typeArgs) {
      resolveTypeNode(t.get(), table);
      if (!t->resolved) {
        Error::internal(t->span, "fail to resolve args Type : " + t->type);
      }
      args.push_back(t->resolved);
    }

    switch (g->gKind) {
    case GenericTypeNode::GenericKind::HANDLE:

      if (args.size() != 1) {
        Error::diagnostic(g->span, "Handle need one type but '" +
                                       to_string(args.size()) + "'");
      }

      if (ar[0]->resolved->kind != TypeSymbol::TypeKind::CLASS) {
        Error::diagnostic(ar[0]->span,
                          "not allowed handle target type : " + ar[0]->type);
      }

      if (ar[0]->resolved->type == Symbol::SymbolType::MAIN) {
        Error::diagnostic(ar[0]->span,
                          "not allowed handle target type : " + ar[0]->type);
      }

      orign = table->getHandle();
      break;
    case GenericTypeNode::GenericKind::OPTION:
      if (args.size() != 1) {
        Error::diagnostic(g->span, "Option need one type but '" +
                                       to_string(args.size()) + "'");
      }
      orign = table->getOption();
      break;
    case GenericTypeNode::GenericKind::RESULT:
      if (args.size() != 2) {
        Error::diagnostic(g->span, "Result neet two type but '" +
                                       to_string(args.size()) + "'");
      }
      if (args[1]->kind != TypeSymbol::TypeKind::ERROR) {
        Error::diagnostic(g->span, "Result's second type is Error but '" +
                                       args[1]->name);
      }
      break;
    }
    g->resolved = table->GenericInsGetOrCreate(orign, args);
  } else {
    Error::diagnostic(type->span, "unknown type : " + type->type);
  }
}

llvm::APInt TypeResolver::resolveFixedArraySize(Expr *expr,
                                                SymbolTable *table) {

  auto lit = dynamic_cast<LiteralExpr *>(expr);
  if (!lit) {
    // TODO: 오류명 맞추기(-는 unary로 들어가서 literal인지로는 에러품질이 좋지
    // 않음)
    Error::diagnostic(expr->span, "array size must be integer literal : ");
  }
  ResolvedLit r;
  switch (lit->token.kind) {
  case TKind::LIT_INT:
    r = TypeResolver::resolveLitInt(lit, table);
    lit->resolvedType = r.type;
    lit->resolvedLit = r;
    break;
  default:
    Error::diagnostic(expr->span, "array size must be integer literal");
  }
  if (!lit) {
    Error::diagnostic(expr->span, "array size must be integer literal");
  }

  llvm::APInt value = lit->resolvedLit.asInt().value;

  if (value.isNegative()) {
    Error::diagnostic(expr->span, "array size cannot be negative");
  }
  if (value == 0) {
    Error::diagnostic(expr->span, "array size must be greater than zero");
  }

  return value.zextOrTrunc(128);
}

static int compareUnsignedDecimal(const std::string &a, const std::string &b) {
  if (a.size() < b.size())
    return -1;
  if (a.size() > b.size())
    return 1;
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  return 0;
}

ResolvedLit TypeResolver::resolveLitInt(LiteralExpr *expr, SymbolTable *table) {
  const string max32 = "2147483647";
  const string max64 = "9223372036854775807";
  const string max128 = "170141183460469231731687303715884105727";

  std::string s = expr->value;

  size_t pos = s.find_first_not_of('0');
  if (pos == std::string::npos) {
    s = "0";
  } else {
    s = s.substr(pos);
  }

  unsigned bits = 0;

  if (compareUnsignedDecimal(s, max32) <= 0) {
    bits = 32;
  } else if (compareUnsignedDecimal(s, max64) <= 0) {
    bits = 64;
  } else if (compareUnsignedDecimal(s, max128) <= 0) {
    bits = 128;
  } else {
    Error::diagnostic(expr->token, "unsupported integer bit width");
  }

  ResolvedLit resolvedLit;

  switch (bits) {
  case 32:
    resolvedLit.type = table->getType("i32");
    break;
  case 64:
    resolvedLit.type = table->getType("i64");
    break;
  case 128:
    resolvedLit.type = table->getType("i128");
    break;
  default:
    Error::internal(expr->token, "invalid integer literal bit width");
  }

  resolvedLit.value = IntPayload(llvm::APInt(bits, llvm::StringRef(s), 10));
  return resolvedLit;
}

bool Helper::checkImpletTraitSig(TypeSymbol *symbol) {

  for (auto &traitType : symbol->traits) {
    auto trait = dynamic_cast<TraitDecl *>(traitType->decl);
    if (!trait) {
      Error::internal("illegal type");
    }
    auto scope = symbol->memberScope;
    for (auto t : trait->traitSigs) {
      auto it = scope->methodMap.find(t->name);
      if (it == scope->methodMap.end()) {
        return false;
      }
      auto &bucket = it->second;
      if (!Helper::hasSameSig(bucket, t->symbol)) {
        return false;
      }
    }
  }

  return true;
}