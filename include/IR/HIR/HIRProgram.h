#pragma once

#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct HIRSource : HIRNode {
  std::string name;

  std::vector<std::unique_ptr<HIRMethodDecl>> methodDecls;
  std::vector<std::unique_ptr<HIRTypeDecl>> typeDecls;
  HIRSource() : HIRNode(HIRNodeKind::Source) {}
};

struct HIRProgram : HIRNode {

  std::vector<HIRSource> sources;
  unordered_map<TypeSymbol *, HIRType *> typeCache;
  unordered_map<TypeSymbol *, HIRTypeDecl *> typeDeclMap;
  HIRVoidType *voidType = nullptr;
  HIRErrorType *errorType = nullptr;

  std::vector<unique_ptr<HIRType>> builtIn;

  HIRProgram(SymbolTable *table) : HIRNode(HIRNodeKind::Program) {
    for (const auto &entry : builtinEntries) {
      std::unique_ptr<HIRBuiltinType> symbol = std::make_unique<HIRBuiltinType>(
          entry.name, entry.category, table->getBuilt(entry.name));
      if (!symbol) {
        Error::internal("failed to create builtin type symbol");
      }
      if (symbol->name.empty()) {
        Error::internal("builtin type symbol has empty name");
      }
      auto raw = symbol.get();
      builtIn.push_back(std::move(symbol));
      typeCache.emplace(table->getBuilt(entry.name), raw);
    }

    builtIn.push_back(make_unique<HIRVoidType>());
    builtIn.push_back(make_unique<HIRVoidType>());
    builtIn.push_back(make_unique<HIRErrorType>());
  }
};
