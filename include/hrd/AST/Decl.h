#pragma once

#include "hrd/AST/ASTNode.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Visitor.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/enums/AccessModifier.h"
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class TypeSymbol;
class ValueSymbol;
class EnumVariantSymbol;
class ImplSymbol;
class MethodSymbol;
class Expr;
class Stmt;
using StmtPtr = shared_ptr<Stmt>;

using ExprPtr = shared_ptr<Expr>;

using namespace std;

// AST에서 모든 선언 노드가 공통으로 상속하는 기반 클래스를 나타낸다.
// 이름과 접근 제어자 정보를 보유하며 선언 계층의 공통 인터페이스를 제공한다.
// 일부 선언은 anonymous를 허용하며 isExtended는 확장 상태를 나타낸다.
class Decl : public ASTNode {
public:
  using Ptr = shared_ptr<Decl>;
  string name; // 대부분의 Decl은 이름을 갖음 (anonymous 경우 빈 문자열 허용)
  AModifier aModifier;
  Decl(NKind k, SourceSpan t, const string &n = "",
       AModifier modi = AModifier::PUBLIC)
      : ASTNode(k, t), name(n), aModifier(modi) {}
  virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  bool isExtended = false;
};

// 변수 선언을 표현하는 AST 노드를 나타낸다.
// 타입, 초기화식, 가변성 및 루트 여부를 포함하며 값 심볼과 연결된다.
// 타입은 항상 존재해야 하며 의미 분석 단계에서 symbol이 설정된다.
class VarDecl : public Decl {
public:
  TypeNode::Ptr type; // 반드시 존재 (타입 추론이면 placeholder)

  ExprPtr init;          // 초기화 식 (없을 수 있음)
  bool isMutable = true; // let vs var 등
  bool isRoot = false;

  DeclContext context;

  VarDecl(SourceSpan t, const string &n, TypeNode::Ptr ty, DeclContext c,
          ExprPtr i = nullptr, bool mut = true, bool ro = false,
          AModifier modi = AModifier::PUBLIC)
      : Decl(NKind::VAR_DECL, t, n, modi), type(std::move(ty)), init(i),
        isMutable(mut), isRoot(ro), context(c) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  ValueSymbol *symbol = nullptr;
};

// 사용금지
// array는 타입 시스템에 종속됨.
class ArrayDecl : public Decl {
public:
  shared_ptr<ArrayTypeNode> type;
  ExprPtr init;
  bool isMutalbe = true;
  bool isRoot = false;
  ArrayDecl(SourceSpan t, const string &n, shared_ptr<ArrayTypeNode> ty,
            ExprPtr i = nullptr, bool m = true, bool r = false,
            AModifier modi = AModifier::PUBLIC)
      : Decl(NKind::ARRAY_DECL, t, n, modi), type(std::move(ty)), init(i),
        isMutalbe(m), isRoot(r) {
    aModifier = modi;
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  ValueSymbol *symbol = nullptr;
  TypeSymbol *baseType = nullptr;
};

// 함수 및 메서드의 파라미터를 표현하는 AST 노드를 나타낸다.
// 이름, 타입, 기본값을 포함하며 값 심볼과 연결된다.
// 기본값은 선택적이며 호출 시점에서 평가되는 것을 전제로 한다.
class Param : public ASTNode {
public:
  string name;
  TypeNode::Ptr type; // param의 타입 (이름은 param에만 있음)
  optional<ExprPtr> defaultValue;

  Param(const string &n, TypeNode::Ptr t, optional<ExprPtr> d = nullopt)
      : ASTNode(NKind::PARAM, t->span), name(n), type(std::move(t)),
        defaultValue(d) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *symbol = nullptr;
};

// 함수 선언을 표현하는 AST 노드를 나타낸다.
// 파라미터, 반환 타입, 본문 및 외부/프레임/오버라이드 속성을 보유한다.
// 의미 분석 이후 methodSymbol과 impl 정보가 연결된다.
class FuncDecl : public Decl {
public:
  vector<shared_ptr<Param>> params;   // 이름 포함된 파라미터
  optional<TypeNode::Ptr> returnType; // 반환 타입 (void면 BuiltinTypeNode void)
  StmtPtr body;
  bool isExtern = false; // 외부 함수 여부 (DLL/FFI 등)
  bool isFrame = false;
  bool isOverride = false;

  FuncDecl(SourceSpan t, const string &n, vector<shared_ptr<Param>> p,
           optional<TypeNode::Ptr> ret, StmtPtr b,
           AModifier modi = AModifier::PUBLIC, bool e = false, bool f = false,
           bool o = false)
      : Decl(NKind::FUNC_DECL, t, n, modi), params(std::move(p)),
        returnType(std::move(ret)), body(std::move(b)), isExtern(e), isFrame(f),
        isOverride(o) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  MethodSymbol *methodSymbol = nullptr;
  ImplSymbol *isImpl = nullptr; // impl타입일 경우에만 할당
};

// 구조체 선언을 표현하는 AST 노드를 나타낸다.
// 필드 목록을 보유하며 값 타입으로서의 데이터 구조를 정의한다.
// 의미 분석 단계에서 TypeSymbol이 연결된다.
class StructDecl : public Decl {
public:
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<InitDecl>> inits;
  StructDecl(SourceSpan t, const string &n, vector<shared_ptr<VarDecl>> f,
             vector<shared_ptr<InitDecl>> i, AModifier modi = AModifier::PUBLIC)
      : Decl(NKind::STRUCT_DECL, t, n, modi), fields(std::move(f)),
        inits(std::move(i)) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

// 클래스 선언을 표현하는 AST 노드를 나타낸다.
// 필드, 메서드, 내부 선언 및 상속/트레이트 정보를 포함한다.
// 단일 상속과 다중 trait 구현을 지원하며 TypeSymbol과 연결된다.
class ClassDecl : public Decl {
public:
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<FuncDecl>> methods;
  vector<shared_ptr<Decl>> innerDecl;
  optional<string> baseClass; // 단일 상속 (필요시 벡터로 변경)
  vector<string> traits;      // trait/interface 목록

  ClassDecl(SourceSpan t, const string &n, vector<shared_ptr<VarDecl>> f,
            vector<shared_ptr<FuncDecl>> m, vector<shared_ptr<Decl>> i,
            optional<string> base = nullopt, vector<string> tr = {},
            AModifier modi = AModifier::PUBLIC)
      : Decl(NKind::CLASS_DECL, t, n, modi), fields(f), methods(m),
        innerDecl(i), baseClass(base), traits(std::move(tr)) {
    aModifier = modi;
  }

  void setBaseClass(const string &b) { baseClass = b; }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

// 열거형 선언을 표현하는 AST 노드를 나타낸다.
// variant 목록과 선택적 payload를 포함하며 aliasing을 위한 baseEnum을 지원한다.
// 각 variant는 별도의 EnumVariantSymbol과 연결된다.
class EnumDecl : public Decl {
public:
  struct Variant {
    Token token;
    string name;
    optional<TypeNode::Ptr>
        payload; // enum variant가 값(튜플 혹은 타입)을 가질 수 있음
    Variant(Token t, const string &n, optional<TypeNode::Ptr> p = nullopt)
        : token(t), name(n), payload(p) {}

    EnumVariantSymbol *symbol = nullptr;
  };

  vector<shared_ptr<Variant>> variants;

  EnumDecl(SourceSpan t, const string &n, vector<shared_ptr<Variant>> v = {},
           AModifier modi = AModifier::PUBLIC)
      : Decl(NKind::ENUM_DECL, t, n, modi), variants(std::move(v)) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

// 특정 타입에 대한 impl 블록을 표현하는 AST 노드를 나타낸다.
// 대상 타입과 구현할 trait 목록 및 연결된 메서드를 보유한다.
// 실제 메서드 바인딩은 이후 단계에서 처리된다.
class ImplDecl : public Decl {
public:
  string target;
  vector<string> traits;
  vector<shared_ptr<FuncDecl>> LinkedImplMethods;

  ImplDecl(SourceSpan t, const string &n, vector<string> tr,
           vector<shared_ptr<FuncDecl>> m, AModifier modi)
      : Decl(NKind::IMPL_DECL, t, "", modi), target(n), traits(std::move(tr)),
        LinkedImplMethods(std::move(m)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  vector<MethodSymbol *> sigs;
};

// trait 선언을 표현하는 AST 노드를 나타낸다.
// 메서드 시그니처 목록을 정의하며 타입의 인터페이스 계약을 구성한다.
// 의미 분석 이후 TypeSymbol로 연결된다.
class TraitDecl : public Decl {
public:
  vector<shared_ptr<TraitSig>> traitSigs;

  TraitDecl(SourceSpan t, const string &n, vector<shared_ptr<TraitSig>> tr,
            AModifier modi)
      : Decl(NKind::TRAIT_DECL, t, n, modi), traitSigs(std::move(tr)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

// trait 내 메서드 시그니처를 표현하는 AST 노드를 나타낸다.
// 반환 타입, 이름, 파라미터를 포함하며 실제 구현 없이 계약만 정의한다.
// 의미 분석 이후 MethodSymbol과 연결된다.
class TraitSig : public ASTNode {
public:
  TypeNode::Ptr type;
  string name;
  vector<shared_ptr<Param>> params;
  TraitSig(SourceSpan t, TypeNode::Ptr ty, const string &n,
           vector<shared_ptr<Param>> p)
      : ASTNode(NKind::TRAIT_SIG, t), type(ty), name(n), params(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  MethodSymbol *symbol = nullptr;
};

// 생성자(init)를 표현하는 특수 함수 선언 노드를 나타낸다.
// 반환 타입 없이 정의되며 오버라이드 여부를 통해 상속 구조를 지원한다.
// 일반 FuncDecl과 동일한 처리 흐름을 따르되 이름이 고정된다.
class InitDecl : public FuncDecl {
public:
  InitDecl(SourceSpan t, vector<shared_ptr<Param>> p, StmtPtr b, bool o = false)
      : FuncDecl(t, "init", p, nullopt, b) {
    isOverride = o;
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};