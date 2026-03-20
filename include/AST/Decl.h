#pragma once

#include "ASTNode.h"
#include "Visitor.h"
#include <memory>
#include <optional>
#include <string>
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
enum class AModifier {
  PUBLIC,
  PROTECTED,
  DEFAULT,
  PRIVATE,
};

class Decl : public ASTNode {
public:
  using Ptr = shared_ptr<Decl>;
  string name; // 대부분의 Decl은 이름을 갖음 (anonymous 경우 빈 문자열 허용)
  AModifier aModifier;
  Decl(NKind k, Token t, const string &n = "",
       AModifier modi = AModifier::DEFAULT)
      : ASTNode(k, t), name(n), aModifier(modi) {}
  virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  bool isExtended = false;
};

// Variable Declaration
class VarDecl : public Decl {
public:
  TypeNode::Ptr type; // 반드시 존재 (타입 추론이면 placeholder)

  ExprPtr init;          // 초기화 식 (없을 수 있음)
  bool isMutable = true; // let vs var 등
  bool isRoot = false;
  VarDecl(Token t, const string &n, TypeNode::Ptr ty, ExprPtr i = nullptr,
          bool mut = true, bool ro = false, AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::VAR_DECL, t, n, modi), type(std::move(ty)), init(i),
        isMutable(mut), isRoot(ro) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  ValueSymbol *symbol = nullptr;
};

class ArrayDecl : public Decl {
public:
  shared_ptr<ArrayTypeNode> type;
  ExprPtr init;
  bool isMutalbe = true;
  bool isRoot = false;
  ArrayDecl(Token t, const string &n, shared_ptr<ArrayTypeNode> ty,
            ExprPtr i = nullptr, bool m = true, bool r = false,
            AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::ARRAY_DECL, t, n, modi), type(std::move(ty)), init(i),
        isMutalbe(m), isRoot(r) {
    aModifier = modi;
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  ValueSymbol *symbol = nullptr;
  TypeSymbol *baseType = nullptr;
};

class Param : public ASTNode {
public:
  string name;
  TypeNode::Ptr type; // param의 타입 (이름은 param에만 있음)
  optional<ExprPtr> defaultValue;
  bool isBorrow = false;

  Param(const string &n, TypeNode::Ptr t, optional<ExprPtr> d = nullopt,
        bool b = false)
      : ASTNode(NKind::PARAM, t->token), name(n), type(std::move(t)),
        defaultValue(d), isBorrow(b) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *symbol = nullptr;
};

// Function Declaration
class FuncDecl : public Decl {
public:
  vector<shared_ptr<Param>> params;   // 이름 포함된 파라미터
  optional<TypeNode::Ptr> returnType; // 반환 타입 (void면 BuiltinTypeNode void)
  StmtPtr body;
  bool isExtern = false; // 외부 함수 여부 (DLL/FFI 등)
  bool isFrame = false;
  bool isOverride = false;

  FuncDecl(Token t, const string &n, vector<shared_ptr<Param>> p,
           optional<TypeNode::Ptr> ret, StmtPtr b,
           AModifier modi = AModifier::DEFAULT, bool e = false, bool f = false,
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

// Struct Declaration
class StructDecl : public Decl {
public:
  vector<shared_ptr<VarDecl>> fields;

  StructDecl(Token t, const string &n, vector<shared_ptr<VarDecl>> f,
             AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::STRUCT_DECL, t, n, modi), fields(std::move(f)) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

// Class Declaration (extends StructDecl with inheritance/visibility)
class ClassDecl : public Decl {
public:
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<FuncDecl>> methods;
  vector<shared_ptr<Decl>> innterDecl;
  optional<string> baseClass; // 단일 상속 (필요시 벡터로 변경)
  vector<string> traits;      // trait/interface 목록

  ClassDecl(Token t, const string &n, vector<shared_ptr<VarDecl>> f,
            vector<shared_ptr<FuncDecl>> m, vector<shared_ptr<Decl>> i,
            optional<string> base = nullopt, vector<string> tr = {},
            AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::CLASS_DECL, t, n, modi), fields(f), methods(m),
        innterDecl(i), baseClass(base), traits(std::move(tr)) {
    aModifier = modi;
  }

  void setBaseClass(const string &b) { baseClass = b; }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

// Enum Declaration
class EnumDecl : public Decl {
public:
  struct Variant {
    Token token;
    string name;
    optional<TypeNode::Ptr>
        payload; // enum variant가 값(튜플 혹은 타입)을 가질 수 있음
    Variant(Token t, const string &n, optional<TypeNode::Ptr> p = nullopt)
        : token(t), name(n), payload(p) {}
  };

  vector<shared_ptr<Variant>> variants;
  optional<string> baseEnum; // for aliasing

  EnumDecl(Token t, const string &n, vector<shared_ptr<Variant>> v = {},
           optional<string> base = nullopt, AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::ENUM_DECL, t, n, modi), variants(std::move(v)),
        baseEnum(base) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

class ImplDecl : public Decl {
public:
  string target;
  vector<string> traits;
  vector<shared_ptr<FuncDecl>> LinkedImplMethods;

  ImplDecl(Token t, const string &n, vector<string> tr,
           vector<shared_ptr<FuncDecl>> m, AModifier modi)
      : Decl(NKind::IMPL_DECL, t, "", modi), target(n), traits(std::move(tr)),
        LinkedImplMethods(std::move(m)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class TraitDecl : public Decl {
public:
  vector<shared_ptr<TraitSig>> traitSigs;

  TraitDecl(Token t, const string &n, vector<shared_ptr<TraitSig>> tr,
            AModifier modi)
      : Decl(NKind::TRAIT_DECL, t, n, modi), traitSigs(std::move(tr)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *symbol = nullptr;
};

class TraitSig : public ASTNode {
public:
  TypeNode::Ptr type;
  string name;
  vector<shared_ptr<Param>> params;
  TraitSig(Token t, TypeNode::Ptr ty, const string &n,
           vector<shared_ptr<Param>> p)
      : ASTNode(NKind::TRAIT_SIG, t), type(ty), name(n), params(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  MethodSymbol *symbol = nullptr;
};

class InitDecl : public FuncDecl {
public:
  InitDecl(Token t, vector<shared_ptr<Param>> p, StmtPtr b, bool o = false)
      : FuncDecl(t, "init", p, nullopt, b) {
    isOverride = o;
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  MethodSymbol *methodSymbol;
};