#pragma once

#include "ASTNode.h"
#include "Expr.h"
#include "Stmt.h"
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
  struct Size {
  public:
    int size = -1;
    pair<int, int> size_f = {-1, -1};
    bool isSigned = false;
    bool isFixed = false;
  } size;
  optional<Expr::Ptr> init; // 초기화 식 (없을 수 있음)
  bool isMutable = true;    // let vs var 등

  VarDecl(Token t, const string &n, TypeNode::Ptr ty, Size s,
          optional<Expr::Ptr> i = nullopt, bool mut = true,
          AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::VAR_DECL, t, n, modi), type(std::move(ty)), size(s),
        init(i), isMutable(mut) {
    aModifier = modi;
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  ValueSymbol *symbol = nullptr;
};

class ArrayDecl : public Decl {
public:
  shared_ptr<ArrayTypeNode> type;
  optional<Expr::Ptr> init;
  bool isMutalbe = true;
  VarDecl::Size size;

  ArrayDecl(Token t, const string &n, shared_ptr<ArrayTypeNode> ty,
            VarDecl::Size s, optional<Expr::Ptr> i = nullopt, bool m = true,
            AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::ARRAY_DECL, t, n, modi), type(std::move(ty)), init(i),
        isMutalbe(m), size(s) {
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
  optional<Expr::Ptr> defaultValue;

  Param(const string &n, TypeNode::Ptr t, optional<Expr::Ptr> d = nullopt)
      : ASTNode(NKind::PARAM, t->token), name(n), type(std::move(t)),
        defaultValue(d) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *symbol = nullptr;
};

// Function Declaration
class FuncDecl : public Decl {
public:
  vector<shared_ptr<Param>> params;   // 이름 포함된 파라미터
  optional<TypeNode::Ptr> returnType; // 반환 타입 (void면 BuiltinTypeNode void)
  Stmt::Ptr body;
  bool isExtern = false; // 외부 함수 여부 (DLL/FFI 등)

  FuncDecl(Token t, const string &n, vector<shared_ptr<Param>> p,
           optional<TypeNode::Ptr> ret, Stmt::Ptr b,
           AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::FUNC_DECL, t, n, modi), params(std::move(p)),
        returnType(std::move(ret)), body(std::move(b)) {
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
  vector<shared_ptr<ASTNode>> body;
  optional<string> baseClass; // 단일 상속 (필요시 벡터로 변경)
  vector<string> traits;      // trait/interface 목록

  ClassDecl(Token t, const string &n, vector<shared_ptr<ASTNode>> b,
            optional<string> base = nullopt, vector<string> tr = {},
            AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::CLASS_DECL, t, n, modi), body(std::move(b)),
        baseClass(base), traits(std::move(tr)) {
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
      : Decl(NKind::IMPL_DECL, t, n, modi), target(n), traits(std::move(tr)),
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
