#include "IR/HIR/HIRBuilder.h"
#include "AST/Expr.h"
#include "AST/Program.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRProgram.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "util/Error.h"

#include <cassert>
#include <memory>
#include <utility>

HIRBuilder::HIRBuilder(SymbolTable *t, HIRProgram *p) : program(p), table(t) {
  source = make_unique<HIRSource>();
}

unique_ptr<HIRSource> HIRBuilder::build(SourceFile *s) {

  for (auto &d : s->decls) {
    lowerTypeShell(d.get());
  }

  for (auto &d : s->decls) {
    d->accept(this);
  }
  return std::move(source);
}

void HIRBuilder::emit(unique_ptr<HIRStmt> stmt) {
  if (currentBlock == nullptr) {
    Error::internal("currentBlock is nullptr");
  }
  if (stmt == nullptr) {
    Error::internal("emit stmt is nullptr");
  }
  currentBlock->statements.push_back(std::move(stmt));
}

HIRType *HIRBuilder::lowerType(TypeSymbol *symbol) {
  HIRType *type = getOrCreateType(symbol);

  if (type == nullptr) {
    Error::internal("fail to get hir type");
  }
  return type;
}

HIRType *HIRBuilder::getOrCreateType(TypeSymbol *symbol) {
  if (symbol == nullptr) {
    Error::internal("symbol is nullptr");
  }
  auto it = program->typeCache.find(symbol);

  if (it != program->typeCache.end()) {
    return it->second;
  } else {
    unique_ptr<HIRType> hirType;
    HIRType *type = nullptr;
    const string &name = symbol->name;
    switch (symbol->kind) {

    case TypeSymbol::TypeKind::CLASS:
      hirType = make_unique<HIREntityType>(name, symbol);
      break;

    case TypeSymbol::TypeKind::ENUM:
      hirType = make_unique<HIREnumType>(name, symbol);
      break;

    case TypeSymbol::TypeKind::STRUCT:
      hirType = make_unique<HIRStructType>(name, symbol);
      break;
    case TypeSymbol::TypeKind::VOID:
      hirType = make_unique<HIRVoidType>();
      break;

    case TypeSymbol::TypeKind::HANDLE:
      if (auto handle = dynamic_cast<GenericSymbol *>(symbol)) {
        hirType = make_unique<HIRHandleType>(
            lowerEntityType(handle->args[0]),
            dynamic_cast<HandleSymbol *>(handle->origin)->storage);
      } else {
        Error::internal("fail to cast handle symbol");
      }
      break;

    case TypeSymbol::TypeKind::RESULT:
      if (auto result = dynamic_cast<GenericSymbol *>(symbol)) {
        auto error = lowerType(result->args[1]);
        if (error->kind != HIRTypeKind::Error) {
          Error::internal("lowered type type is not error");
        }
        hirType = make_unique<HIRResultType>(
            lowerType(result->args[0]), dynamic_cast<HIRErrorType *>(error));
      } else {
        Error::internal("fail to cast result symbol");
      }
      break;

    case TypeSymbol::TypeKind::OPTION:
      if (auto option = dynamic_cast<GenericSymbol *>(symbol)) {
        hirType = make_unique<HIROptionType>(lowerType(option->args[0]));
      } else {
        Error::internal("fail to cast option symbol");
      }
      break;

    case TypeSymbol::TypeKind::ERROR:
      hirType = make_unique<HIRErrorType>();
      break;

    default:
      Error::internal("illegal type symbol kind");
    }

    if (hirType == nullptr) {
      Error::internal("fail to make ptr");
    }

    type = hirType.get();
    ownedTypes.push_back(std::move(hirType));
    program->typeCache.emplace(symbol, type);
    return type;
  }
}

HIRLocal *HIRBuilder::lookUpLocal(ValueSymbol *symbol) {
  assert(currentBlock);
  auto it = currentBlock->localMap.find(symbol);
  if (it == currentBlock->localMap.end()) {
    return nullptr;
  }
  return it->second;
}

HIRLocal *HIRBuilder::makeTemp(HIRType *type) {
  auto local = make_unique<HIRLocal>();
  local->id = allocLocalID();
  local->type = type;
  local->symbol = nullptr;
  local->kind = HIRLocalKind::Temp;
  local->isMutable = false;
  local->isInitialized = false;
  local->name = "";
  auto raw = local.get();

  currentMethod->locals.push_back(std::move(local));

  return raw;
}