#pragma once

#include "./../DebugColors.h"

#include "ASTNode.h"
#include "Node.h"
#include "Stmt.h"
#include <memory>
#include <string>
#include <vector>

using namespace std;

// ────────────────────────────────
// Type Node (TYPE_NODE)
enum class TypeCategory { SIMPLE, GENERIC, REFERENCE, ARRAY };

class TypeNode : public ASTNode {
public:
  std::string type;
  TypeCategory category;
  std::vector<std::shared_ptr<TypeNode>> subTypes;

  TypeNode(const std::string &n, TypeCategory cat = TypeCategory::SIMPLE,
           const std::vector<std::shared_ptr<TypeNode>> &sub = {})
      : ASTNode(NodeKind::TYPE_NODE), type(n), category(cat), subTypes(sub) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class FixedNode : public TypeNode {
public:
  pair<int, int> size;
  FixedNode(const std::string &n, const pair<int, int> s)
      : TypeNode(n), size(s) {
        if(size.first==-1){
          size={16,16};
        }
      }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class SimpleTypeNode : public TypeNode {
public:
  int size;
  bool isUnsigned;
  SimpleTypeNode(const std::string &n, const int s, const bool u)
      : TypeNode(n), size(s), isUnsigned(u) {
        if(n=="int"){
          
        }
      }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Class Declaration (CLASS_DECL)
class ClassDecl : public Stmt {
public:
  std::string name;
  std::vector<Ptr> members; // 멤버 변수, 함수 등

  ClassDecl(const std::string &n, std::vector<Ptr> m)
      : Stmt(NodeKind::CLASS_DECL), name(n), members(m) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Struct Declaration (STRUCT_DECL)
class StructDecl : public Stmt {
public:
  std::string name;
  std::vector<ASTNode *> members;

  StructDecl(const std::string &n) : Stmt(NodeKind::STRUCT_DECL), name(n) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Enum Declaration (ENUM_DECL)
class EnumDecl : public Stmt {
public:
  std::string name;
  std::vector<std::string> values;

  EnumDecl(const std::string &n, const std::vector<std::string> &vals)
      : Stmt(NodeKind::ENUM_DECL), name(n), values(vals) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Interface Declaration (INTERFACE_DECL)
class InterfaceDecl : public Stmt {
public:
  std::string name;
  std::vector<std::shared_ptr<FuncDecl>> methods;

  InterfaceDecl(const std::string &n)
      : Stmt(NodeKind::INTERFACE_DECL), name(n) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class VarDecl : public Stmt {
public:
  std::shared_ptr<TypeNode> type;
  std::string name;
  Expr::Ptr init;

  VarDecl(std::shared_ptr<TypeNode> t, std::string n, Expr::Ptr i, TokKind k)
      : Stmt(NodeKind::VAR_DECL), type(t), name(n), init(i) {
    if (init == nullptr) {
      init = make_shared<LiteralExpr>("null", k);
    }
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ArrayDecl : public Stmt {
public:
  std::shared_ptr<TypeNode> type;
  std::string name;
  Expr::Ptr index;
  Expr::Ptr init;
  ArrayDecl(std::shared_ptr<TypeNode> t, std::string n, Expr::Ptr i,
            Expr::Ptr in = nullptr)
      : Stmt(NodeKind::ARRAY_DECL), type(t), name(n), index(i), init(in) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class FuncDecl : public Stmt {
public:
  class Param {
  public:
    std::string name;
    std::shared_ptr<TypeNode> type;
    LiteralExpr value;

    Param(const std::string &n, std::shared_ptr<TypeNode> t, LiteralExpr v)
        : name(n), type(std::move(t)), value(v) {}
  };

public:
  std::string name;
  std::vector<shared_ptr<Param>> params;
  shared_ptr<TypeNode> returnType;
  vector<shared_ptr<Stmt>> members;
  bool isDynamic;

  FuncDecl(const std::string &n, const std::vector<shared_ptr<Param>> &p,
           shared_ptr<TypeNode> ret, vector<shared_ptr<Stmt>> m,
           bool dy = false)
      : Stmt(NodeKind::FUNC_DECL), name(n), params(p), returnType(ret),
        members(m), isDynamic(dy) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Program Node (PROGRAM)
class Program : public ASTNode {
public:
  std::vector<Stmt::Ptr> declarations;
  Stmt::Ptr mainClass;

  Program(std::vector<Stmt::Ptr> d, Stmt::Ptr m = nullptr)
      : ASTNode(NodeKind::PROGRAM), declarations(d), mainClass(m) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};
