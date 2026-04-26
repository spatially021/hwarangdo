#pragma once

#include "AST/Decl.h"
#include "AST/Program.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

// HIR 단위의 최상위 소스 컨테이너를 나타낸다.
// 메서드 선언과 타입 선언을 소유하며 프로그램 구성의 루트 단위로 사용된다.
// 내부 요소들의 생명주기는 HIRSource에 종속된다.
struct HIRSource : HIRNode {
  std::string name;

  std::vector<std::unique_ptr<HIRMethodDecl>> methodDecls;
  std::vector<std::unique_ptr<HIRTypeDecl>> typeDecls;
  std::vector<std::unique_ptr<HIRType>> types;
  vector<unique_ptr<HIRHandleType>> handles;
  vector<unique_ptr<HIRObserverType>> observers;
  SourceFile *source = nullptr;
  HIRSource(SourceFile *s) : HIRNode(HIRNodeKind::Source), source(s) {}
};

// HIR 전체를 관리하는 프로그램 루트 컨테이너를 나타낸다.
// 소스 단위, 타입 캐시, 타입 선언 매핑 및 내장 타입을 초기화하고 보유한다.
// 타입 객체의 일관성과 공유를 보장하며 built-in 타입은 생성 시점에 캐시에
// 등록된다.
struct HIRProgram : HIRNode {

  std::vector<unique_ptr<HIRSource>> sources;

  unordered_map<TypeSymbol *, HIRType *> typeCache;
  unordered_map<TypeSymbol *, HIRTypeDecl *> typeDeclMap;
  unordered_map<EnumVariantSymbol *, HIREnumVariant *> variantMap;
  unordered_map<HIREntityType *, HIRHandleType *> handleCache;
  unordered_map<HIREntityType *, HIRObserverType *> observerCache;

  std::vector<unique_ptr<HIRField>> roots;
  unordered_map<ValueSymbol *, HIRField *> rootMap;

  Scope *rootScope = nullptr;

  HIRVoidType *voidType = nullptr;
  HIRErrorType *errorType = nullptr;
  HIRDefaultType *defaultType = nullptr;

  HIRType *rootType = nullptr;

  std::vector<unique_ptr<HIRType>> builtIn;

  int nextRootId = 0;

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

    unique_ptr<HIRVoidType> vt = make_unique<HIRVoidType>();
    voidType = vt.get();
    builtIn.push_back(std::move(vt));
    typeCache.emplace(table->getBuilt("void"), voidType);

    unique_ptr<HIRErrorType> et = make_unique<HIRErrorType>();
    errorType = et.get();
    builtIn.push_back(std::move(et));

    unique_ptr<HIRDefaultType> dt = make_unique<HIRDefaultType>();
    defaultType = dt.get();
    builtIn.push_back(std::move(dt));

    rootScope = table->rootScope.get();
    auto rt = make_unique<HIRType>(HIRTypeKind::Root, "root");
    rootType = rt.get();
    builtIn.push_back(std::move(rt));
  }

  void linkRoot() {
    for (auto &s : rootScope->value) {
      auto decl = dynamic_cast<VarDecl *>(s.second->node);
      if (decl == nullptr) {
        Error::internal("root decl but not varDecl");
      }

      unique_ptr<HIRField> field = make_unique<HIRField>();
      field->symbol = decl->symbol;
      field->name = decl->name;
      field->isInitialized = (decl->init != nullptr);
      field->isMutable = decl->isMutable;
      auto it = typeCache.find(decl->type->resolved);
      if (it == typeCache.end()) {
        Error::internal("unknown type");
      }
      auto type = it->second;
      if (type == nullptr) {
        Error::internal("type is nullptr");
      }
      field->type = type;
      field->id = nextRootId++;
      auto raw = field.get();
      roots.push_back(std::move(field));
      rootMap.emplace(decl->symbol, raw);
    }
  }
};