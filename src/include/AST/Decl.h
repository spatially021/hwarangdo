#pragma once

#include "ASTNode.h"
#include "Stmt.h"
#include "Expr.h"
#include "Visitor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
  Decl(NKind kind, Token token, const string &n = "",AModifier modi=AModifier::DEFAULT)
      : ASTNode(kind, token), name(n) {}
  virtual void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// Variable Declaration
class VarDecl : public Decl {
public:
  TypeNode::Ptr type;       // 반드시 존재 (타입 추론이면 placeholder)
  optional<Expr::Ptr> init; // 초기화 식 (없을 수 있음)
  bool isMutable = true;    // let vs var 등

  VarDecl(Token t, const string &n, TypeNode::Ptr ty,
          optional<Expr::Ptr> i = nullopt, bool mut = true,
          AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::VAR_DECL, t, n,modi), type(std::move(ty)), init(i),
        isMutable(mut) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// Function Declaration
class FuncDecl : public Decl {
public:
  struct Param {
    string name;
    TypeNode::Ptr type; // param의 타입 (이름은 param에만 있음)
    optional<Expr::Ptr> defaultValue;

    Param(const string &n, TypeNode::Ptr t, optional<Expr::Ptr> d = nullopt)
        : name(n), type(std::move(t)), defaultValue(d) {}
  };

  vector<Param> params;             // 이름 포함된 파라미터
  TypeNode::Ptr returnType;         // 반환 타입 (void면 BuiltinTypeNode void)
  vector<shared_ptr<ASTNode>> body; // 함수 몸체 (Stmt/Decl 등)
  bool isMethod = false;            // 클래스/struct의 메서드 여부
  bool isExtern = false;            // 외부 함수 여부 (DLL/FFI 등)

  FuncDecl(Token t, const string &n, vector<Param> p, TypeNode::Ptr ret,
           vector<shared_ptr<ASTNode>> b = {}, bool method = false,
           AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::FUNC_DECL, t, n, modi), params(std::move(p)),
        returnType(std::move(ret)), body(std::move(b)), isMethod(method) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};


// Struct Declaration
class StructDecl : public Decl {
public:
  vector<shared_ptr<VarDecl>> fields;
  vector<string> traits;

  StructDecl(Token t, const string &n, vector<shared_ptr<VarDecl>> f = {},
             vector<string> tr = {},
             AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::STRUCT_DECL, t, n, modi), fields(std::move(f)),
        traits(std::move(tr)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
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
        traits(std::move(tr)), baseClass(base) {}

  void setBaseClass(const string &b) { baseClass = b; }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// Enum Declaration
class EnumDecl : public Decl {
public:
  struct Variant {
    string name;
    optional<TypeNode::Ptr>
        payload; // enum variant가 값(튜플 혹은 타입)을 가질 수 있음
    Variant(const string &n, optional<TypeNode::Ptr> p = nullopt)
        : name(n), payload(std::move(p)) {}
  };

  vector<Variant> variants;
  optional<string> baseEnum; // for aliasing / underlying type

  EnumDecl(Token t, const string &n, vector<Variant> v = {},
           optional<string> base = nullopt, AModifier modi = AModifier::DEFAULT)
      : Decl(NKind::ENUM_DECL, t, n, modi), variants(std::move(v)),
        baseEnum(base) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};
