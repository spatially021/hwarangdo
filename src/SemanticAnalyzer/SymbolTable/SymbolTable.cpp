#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/AST/ASTNode.h"
#include "hrd/BuiltInType.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <llvm/ADT/APInt.h>
#include <memory>
#include <string>
#include <utility>

using scopePtr = shared_ptr<Scope>;

SymbolTable::SymbolTable() {

  for (const auto &entry : builtinEntries) {
    std::unique_ptr<PrimtiveType> symbol;

    switch (entry.category) {
    case BuiltinCategory::Int:
      symbol = std::make_unique<IntType>(entry.type);
      break;

    case BuiltinCategory::Float:
      symbol = std::make_unique<FloatType>(entry.type);
      break;

    case BuiltinCategory::Char:
      symbol = std::make_unique<CharType>(entry.type);
      break;

    case BuiltinCategory::String:
      symbol = std::make_unique<StringType>(entry.type);
      break;
    case BuiltinCategory::Bool:
      symbol = std::make_unique<BoolType>();
      break;
    default:
      continue;
    }
    if (!symbol) {
      Error::internal("failed to create builtin type symbol");
    }
    if (symbol->name.empty()) {
      Error::internal("builtin type symbol has empty name");
    }
    registry.addBuilt(std::move(symbol));
  }
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = "void";
  symbol->kind = TypeKind::VOID;
  registry.addBuilt(std::move(symbol));

  symbol = make_unique<HandleSymbol>();
  symbol->name = "Handle";
  symbol->kind = TypeKind::HANDLE;
  registry.addBuilt(std::move(symbol));

  symbol = make_unique<ResultSymbol>();
  symbol->name = "Result";
  symbol->kind = TypeKind::RESULT;
  registry.addBuilt(std::move(symbol));

  symbol = make_unique<OptionSymbol>();
  symbol->name = "Option";
  symbol->kind = TypeKind::OPTION;
  registry.addBuilt(std::move(symbol));

  symbol = make_unique<ErrorType>();
  symbol->name = "Error";
  symbol->kind = TypeKind::ERROR;
  registry.addBuilt(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "@built";
  symbol->kind = TypeKind::BUILTIN;
  registry.addBuilt(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "@default";
  symbol->kind = TypeKind::DEFAULT_VALUE;
  registry.addBuilt(std::move(symbol));

  scopeManger.setCurrentToToplevel();
}

SymbolTable::~SymbolTable() = default;

Result SymbolTable::add(unique_ptr<Symbol> symbol) {
  assert(symbol != nullptr);
  switch (symbol->type) {
  case Symbol::SymbolType::TYPE: {
    std::unique_ptr<TypeSymbol> s(static_cast<TypeSymbol *>(symbol.release()));
    if (auto type = getType(s->name); type != nullptr) {
      if (s->isReserved)
        return {false, Result::RESERVED, type->decl->span};
      return {false, Result::DUPLICATED, type->decl->span};
    }

    auto ok = registry.addType(std::move(s));
    return {ok.first, ok.first ? Result::NONE : Result::DUPLICATED, ok.second};
  }
  case Symbol::SymbolType::VALUE: {
    auto b = scopeManger.addValue(
        unique_ptr<ValueSymbol>(static_cast<ValueSymbol *>(symbol.release())));
    return Result(
        {b.first, (b.first ? Result::NONE : Result::DUPLICATED), b.second});
  }

  case Symbol::SymbolType::METHOD: {
    auto b = registry.addMethod(unique_ptr<MethodSymbol>(
        static_cast<MethodSymbol *>(symbol.release())));
    return Result(
        {b.first, (b.first ? Result::NONE : Result::DUPLICATED), b.second});
  }

  case Symbol::SymbolType::MAIN: {
    auto b = registry.addType(
        unique_ptr<MainSymbol>(static_cast<MainSymbol *>(symbol.release())));

    return Result(
        {b.first, (b.first ? Result::NONE : Result::DUPLICATED), b.second});
  }

  default:
    return Result({false, Result::UNKNOWN_SYMBOL, {}});
  }
}

TypeSymbol *SymbolTable::getType(str name) {
  assert(registry.getCurrentFile() &&
         "getType requires an active file context");
  return registry.getType(name);
}

TypeSymbol *SymbolTable::getBuilt(BuiltInType type) {
  switch (type) {
  case BuiltInType::I8:
    return registry.getBuilt("i8");
  case BuiltInType::I16:
    return registry.getBuilt("i16");
  case BuiltInType::I32:
    return registry.getBuilt("i32");
  case BuiltInType::I64:
    return registry.getBuilt("i64");
  case BuiltInType::I128:
    return registry.getBuilt("i128");

  case BuiltInType::U8:
    return registry.getBuilt("u8");
  case BuiltInType::U16:
    return registry.getBuilt("u16");
  case BuiltInType::U32:
    return registry.getBuilt("u32");
  case BuiltInType::U64:
    return registry.getBuilt("u64");
  case BuiltInType::U128:
    return registry.getBuilt("u128");

  case BuiltInType::F16:
    return registry.getBuilt("f16");
  case BuiltInType::F32:
    return registry.getBuilt("f32");
  case BuiltInType::F64:
    return registry.getBuilt("f64");
  case BuiltInType::F128:
    return registry.getBuilt("f128");

  case BuiltInType::C8:
    return registry.getBuilt("c8");
  case BuiltInType::C16:
    return registry.getBuilt("c16");
  case BuiltInType::C32:
    return registry.getBuilt("c32");

  case BuiltInType::S8:
    return registry.getBuilt("s8");
  case BuiltInType::S16:
    return registry.getBuilt("s16");
  case BuiltInType::S32:
    return registry.getBuilt("s32");

  case BuiltInType::B:
    return registry.getBool();

  case BuiltInType::FI:
    return registry.getBuilt("fi");

  case BuiltInType::VOID:
    return registry.getBuilt("void");
  }

  return nullptr;
}

TypeSymbol *SymbolTable::getType(TypeNode *node) {
  if (node->kind == NKind::BUILT_IN_TYPE) {
    auto b = static_cast<BuiltinTypeNode *>(node);
    switch (b->type) {
    case BuiltInType::I8:
      return registry.getBuilt("i8");
    case BuiltInType::I16:
      return registry.getBuilt("i16");
    case BuiltInType::I32:
      return registry.getBuilt("i32");
    case BuiltInType::I64:
      return registry.getBuilt("i64");
    case BuiltInType::I128:
      return registry.getBuilt("i128");
    case BuiltInType::U8:
      return registry.getBuilt("u8");
    case BuiltInType::U16:
      return registry.getBuilt("u16");
    case BuiltInType::U32:
      return registry.getBuilt("u32");
    case BuiltInType::U64:
      return registry.getBuilt("u64");
    case BuiltInType::U128:
      return registry.getBuilt("u128");
    case BuiltInType::F16:
      return registry.getBuilt("f16");
    case BuiltInType::F32:
      return registry.getBuilt("f32");
    case BuiltInType::F64:
      return registry.getBuilt("f64");
    case BuiltInType::F128:
      return registry.getBuilt("f128");
    case BuiltInType::C8:
      return registry.getBuilt("c8");
    case BuiltInType::C16:
      return registry.getBuilt("c16");
    case BuiltInType::C32:
      return registry.getBuilt("c32");
    case BuiltInType::B:
      return registry.getBuilt("bool");
    case BuiltInType::FI:
      return registry.getBuilt("fixed");
    case BuiltInType::S8:
      return registry.getBuilt("s8");
    case BuiltInType::S16:
      return registry.getBuilt("s16");
    case BuiltInType::S32:
      return registry.getBuilt("s32");
    case BuiltInType::VOID:
      return registry.getBuilt("void");
    }
  } else {
    return getType(node->type);
  }
  return nullptr;
}

TypeSymbol *SymbolTable::getCommonNumbericType(TypeSymbol *left,
                                               TypeSymbol *right) {

  if (isa<IntType>(left) && isa<IntType>(right)) {
    return registry.getBuilt("int");
  }

  if (left == registry.getBuilt("float") &&
      right == registry.getBuilt("float")) {
    return registry.getBuilt("float");
  }
  return nullptr;
}