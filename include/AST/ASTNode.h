#pragma once

#include "BuiltInType.h"
#include "Token.h"
#include "Visitor.h"
#include <memory>
#include <optional>
#include <vector>

class TypeSymbol;

enum class NKind {
  // Expressions
  LITERAL_EXPR,
  VAR_EXPR,
  BINARY_EXPR,
  UNARY_EXPR,
  CALL_EXPR,
  GROUP_EXPR,
  ASSIGN_EXPR,
  MEMBER_EXPR,
  ARRAY_ACCESS_EXPR,
  POSTFIX_EXPR,
  TERNARY_EXPR,
  NEW_EXPR,
  THIS_EXPR,
  SUPER_EXPR,
  MATCH_EXPR,
  ENUM_VARIANT_EXPR,
  MOVE_EXPR,
  BORROW_EXPR,
  REFERENCE_EXPR,

  // Statements
  EXPR_STMT,
  VAR_STMT,
  BLOCK_STMT,
  IF_STMT,
  FOR_STMT,
  WHILE_STMT,
  SWITCH_STMT,
  DEFAULT_STMT,
  RETURN_STMT,
  BREAK_STMT,
  CONTINUE_STMT,
  EMPTY_STMT,
  PARAM_STMT,
  DECL_STMT,
  TRY_STMT,
  CATCH_STMT,
  THROW_STMT,
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

  // others
  TRAIT_SIG,
  MATCH_CASE,
  SWITCH_CASE,
  RANGE,
  PARAM,

};
class ASTVisitor;
class Expr;
using ExprPtr = shared_ptr<Expr>;

class ASTNode {
public:
  NKind kind;
  Token token;

  ASTNode(NKind k, Token t) : kind(k), token(t) {}

  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor *visitor) = 0;
};

class TypeNode : public ASTNode {
public:
  using Ptr = shared_ptr<TypeNode>;

  string type;
  virtual ~TypeNode() = default;
  TypeNode(NKind k, Token t) : ASTNode(k, t) { type = t.text; }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *resolved = nullptr;
};

class BuiltinTypeNode : public TypeNode {
public:
  enum class Category {
    Int,
    Float,
    Fixed,
    Bool,
    CHAR,
    STRING,
    Void,
    FUNC
  } category;

  BuiltInType type = {};

  bool isSigned = true;

  BuiltinTypeNode(Token t, Category c, Token s = {})
      : TypeNode(NKind::BUILT_IN_TYPE, t), category(c) {
    if (s.kind == TKind::EMPTY) {
      switch (category) {
      case Category::Int:
        type = BuiltInType::I32;
        break;
      case Category::Float:
        type = BuiltInType::F32;
        break;
      case Category::CHAR:
      case Category::STRING:
        type = BuiltInType::C8;
        break;
      default:
        break;
      }
    } else {
      auto size = s.text;
      if (size == "i8") {
        type = BuiltInType::I8;
      } else if (size == "i16") {
        type = BuiltInType::I16;
      } else if (size == "i32") {
        type = BuiltInType::I32;
      } else if (size == "i64") {
        type = BuiltInType::I64;
      } else if (size == "i128") {
        type = BuiltInType::I128;
      } else if (size == "f16") {
        type = BuiltInType::F16;
      } else if (size == "f32") {
        type = BuiltInType::F32;
      } else if (size == "f64") {
        type = BuiltInType::F64;
      } else if (size == "f128") {
        type = BuiltInType::F128;
      } else if (size == "u8") {
        type = BuiltInType::U8;
      } else if (size == "u16") {
        type = BuiltInType::U16;
      } else if (size == "u32") {
        type = BuiltInType::U32;
      } else if (size == "u64") {
        type = BuiltInType::U64;
      } else if (size == "u128") {
        type = BuiltInType::U128;
      } else if (size == "c8") {
        type = BuiltInType::C8;
      } else if (size == "c16") {
        type = BuiltInType::C16;
      } else if (size == "c32") {
        type = BuiltInType::C32;
      }
    }
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
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
      : TypeNode(NKind::REFERENCE_TYPE, t), target(std::move(trg)),
        isMutable(mut) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ArrayTypeNode : public TypeNode {
public:
  TypeNode::Ptr elementType;
  optional<ExprPtr> fixedSize; // nullopt => dynamic / slice
  ArrayTypeNode(Token t, TypeNode::Ptr elem, optional<ExprPtr> size = nullopt)
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
      : TypeNode(NKind::GENERIC_TYPE, t), baseName(base),
        typeArgs(std::move(args)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};
