#pragma once

#include "hrd/AST/Decl.h"
#include "hrd/AST/Program.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"
#include <memory>
#include <string>
#include <vector>

// HIR 단위의 최상위 소스 컨테이너를 나타낸다.
// 메서드 선언과 타입 선언을 소유하며 프로그램 구성의 루트 단위로 사용된다.
// 내부 요소들의 생명주기는 HIRSource에 종속된다.
struct HIRSource : HIRNode {
  std::string name;

  std::vector<std::unique_ptr<HIRMethodDecl>> methodDecls;
  std::vector<std::unique_ptr<HIRTypeDecl>> typeDecls;
  std::vector<std::unique_ptr<TypeSymbol>> types;
  vector<unique_ptr<TypeSymbol>> handles;
  vector<unique_ptr<TypeSymbol>> observers;
  SourceFile *source = nullptr;
  HIRSource(SourceSpan sp, SourceFile *s)
      : HIRNode(sp, HIRNodeKind::Source), source(s) {}
};

// HIR 전체를 관리하는 프로그램 루트 컨테이너를 나타낸다.
// 소스 단위, 타입 캐시, 타입 선언 매핑 및 내장 타입을 초기화하고 보유한다.
// 타입 객체의 일관성과 공유를 보장하며 built-in 타입은 생성 시점에 캐시에
// 등록된다.
struct HIRProgram : HIRNode {

  std::vector<unique_ptr<HIRSource>> sources;

  unordered_map<TypeSymbol *, HIRTypeDecl *> typeDeclMap;

  Scope *rootScope = nullptr;

  int nextRootId = 0;

  SymbolTable &table;

  HIRProgram(SourceSpan s, SymbolTable &t)
      : HIRNode(s, HIRNodeKind::Program), table(t) {}
};