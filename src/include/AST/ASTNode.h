#pragma once

#include "../Token.h"
#include "Visitor.h"
#include <algorithm>
#include <memory>
#include <optional>

enum class NKind {
  // Expressions
  LITERAL_EXPR,
  VAR_EXPR,
  BINARY_EXPR,
  UNARY_EXPR,
  CALL_EXPR,
  GROUP_EXPR,
  ASSIGN_EXPR,
  ACCESS_EXPR,
  INDEX_EXPR,
  POSTFIX_EXPR,
  TERNARY_EXPR,
  NEW_EXPR,
  THIS_EXPR,
  SUPER_EXPR,

  // Statements
  EXPR_STMT,
  VAR_STMT,
  BLOCK_STMT,
  IF_STMT,
  FOR_STMT,
  WHILE_STMT,
  SWITCH_STMT,
  CASE_STMT,
  DEFAULT_STMT,
  RETURN_STMT,
  BREAK_STMT,
  CONTINUE_STMT,
  EMPTY_STMT,
  PARAM_STMT,
  DECL_STMT,
  TRY_STMT,
  CATCH_STMT,
  ONEXIT_STMT,


  // Declarations
  CLASS_DECL,
  STRUCT_DECL,
  IMPL_DECL,
  TRAIT_DECL,
  ENUM_DECL,
  FUNC_DECL,
  VAR_DECL,
  ARRAY_DECL,


  // Types
  TYPE_NODE,
  BUILT_IN_TYPE,
  IDENTIFIER_TYPE,
  REFERENCE_TYPE,
  ARRAY_TYPE,
  FUNCTION_TYPE,
  GENERIC_TYPE,


};
class ASTVisitor;

class ASTNode{
public:
    NKind kind;
    Token token;
    
    ASTNode(NKind k,Token t):kind(k),token(t){}

    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor *visitor) = 0;
};


class TypeNode : public ASTNode {
public:
  using Ptr = shared_ptr<TypeNode>;
  
  virtual ~TypeNode() = default;
  TypeNode(NKind kind, Token t) : ASTNode(kind, t) {}
  

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BuiltinTypeNode : public TypeNode {
public:
  enum class Category { Int, Float, Fixed, Bool,CHAR,STRING, Void };
  Category category;
  int bitWidth = 0; // int/uint/float 용
  int intBits = 0;  // fixed 전용
  int fracBits = 0; // fixed 전용

  bool isSigned=true;

  BuiltinTypeNode(Token t, Category c)
      : TypeNode(NKind::BUILT_IN_TYPE, t), category(c) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  void setSize(int i){
    if(i==-1){
      switch (category) {

      case Category::Int:
      bitWidth=32;
      case Category::Float:
      bitWidth=32;
      case Category::CHAR:
      bitWidth=8;
      case Category::STRING:
      bitWidth=8;
      default:
        break;
      }
    }else bitWidth=i;}
  void setSize(pair<int,int> i){
    if(i.first==-1){
      intBits=fracBits=16;
    }else intBits=i.first;fracBits=i.second;}
};

class IdentifierTypeNode : public TypeNode {
public:
  string name; // ex: Player, Transform
  IdentifierTypeNode(Token t, const string &n)
      : TypeNode(NKind::IDENTIFIER_TYPE, t), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ReferenceTypeNode : public TypeNode {
public:
  TypeNode::Ptr target;
  bool isMutable = false; // &T vs &mut T
  ReferenceTypeNode(Token t, TypeNode::Ptr trg, bool mut = false)
      : TypeNode(NKind::REFERENCE_TYPE, t), target(std::move(trg)), isMutable(mut) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ArrayTypeNode : public TypeNode {
public:
  TypeNode::Ptr elementType;
  optional<int> fixedSize; // nullopt => dynamic / slice
  ArrayTypeNode(Token t, TypeNode::Ptr elem, optional<int> size = nullopt)
      : TypeNode(NKind::ARRAY_TYPE, t), elementType(std::move(elem)),
        fixedSize(size) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class FunctionTypeNode : public TypeNode {
public:
  vector<TypeNode::Ptr> paramTypes; // 이름 없음, 타입 시그니처만
  TypeNode::Ptr returnType;
  FunctionTypeNode(Token t, vector<TypeNode::Ptr> params, TypeNode::Ptr ret)
      : TypeNode(NKind::FUNCTION_TYPE, t), paramTypes(std::move(params)),
        returnType(std::move(ret)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class GenericTypeNode : public TypeNode {
public:
  string baseName;
  vector<TypeNode::Ptr> typeArgs;
  GenericTypeNode(Token t, const string &base, vector<TypeNode::Ptr> args)
      : TypeNode(NKind::GENERIC_TYPE, t), baseName(base), typeArgs(std::move(args)) {
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};
