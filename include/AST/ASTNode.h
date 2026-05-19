#pragma once

#include "BuiltInType.h"
#include "SourceSpan.h"
#include "Token.h"
#include "Visitor.h"
#include "util/Error.h"
#include <memory>
#include <vector>

class TypeSymbol;

enum class NKind {
  // Expressions
  LITERAL_EXPR,
  NAME_EXPR,
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
  SELF_EXPR,
  ROOT_EXPR,

  MATCH_EXPR,
  ENUM_VARIANT_EXPR,
  CAST_EXPR,
  BUILTIN_NAME_EXPR,
  SPAWN_EXPR,
  VIEW_EXPR,
  DESTROY_EXPR,
  DEFUALT_VALUE_EXPR,
  VALUE_EXPR,

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
  VALUE_TRANSFER_STMT,
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
  INIT_DECL,

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

// AST의 모든 노드가 공통으로 상속하는 기반 클래스를 나타낸다.
// 노드 종류와 토큰 정보를 보유하며 visitor 패턴을 통한 순회를 지원한다.
// 모든 파생 노드는 accept를 구현해야 하며 다형적 소멸을 보장한다.
class ASTNode {
public:
  NKind kind;
  SourceSpan span;
  ASTNode(NKind k, SourceSpan s) : kind(k), span(s) {}

  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor *visitor) = 0;
};

// 타입을 표현하는 AST 노드의 공통 기반 클래스를 나타낸다.
// 토큰 기반 타입 문자열과 의미 분석 결과(TypeSymbol)를 보유한다.
// 모든 타입 노드는 resolved가 채워지는 것을 전제로 이후 단계에서 사용된다.
class TypeNode : public ASTNode {
public:
  using Ptr = shared_ptr<TypeNode>;

  string type;
  virtual ~TypeNode() = default;
  TypeNode(NKind k, Token t) : ASTNode(k, t.span) { type = t.text; }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *resolved = nullptr;
  bool setSize = false;
};
// 내장 타입을 표현하는 AST 노드를 나타낸다.
// 카테고리와 크기 정보를 기반으로 구체적인 BuiltInType을 결정한다.
// 크기 토큰이 없을 경우 기본 타입으로 초기화되며 잘못된 값은 허용되지 않는다.
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
    if (s.text == "") {
      setSize = false;
      switch (category) {
      case Category::Int:
        type = BuiltInType::I32;
        break;
      case Category::Float:
        type = BuiltInType::F32;
        break;
      case Category::CHAR:
        type = BuiltInType::C8;
        break;
      case Category::STRING:
        type = BuiltInType::S8;
        break;
      case Category::Bool:
        type = BuiltInType::B;
        break;
      case Category::Void:
        type = BuiltInType::VOID;
        break;
      default:
        Error::internal(t, "unknown builtInType");
        break;
      }
    } else {
      auto size = s.text;
      setSize = true;
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

// 사용자 정의 타입 이름을 참조하는 AST 노드를 나타낸다.
// 식별자 문자열을 기반으로 의미 분석 단계에서 실제 타입으로 해석된다.
// resolved 필드는 해당 타입 심볼로 채워지는 것을 전제로 한다.
class IdentifierTypeNode : public TypeNode {
public:
  string name; // ex: Player, Transform
  IdentifierTypeNode(Token s, const string &n)
      : TypeNode(NKind::IDENTIFIER_TYPE, s), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// 배열 타입을 표현하는 AST 노드를 나타낸다.
// 요소 타입과 선택적 고정 크기를 보유하며 크기가 없으면 동적 배열로 간주된다.
// fixedSize는 표현식 형태로 유지되며 이후 단계에서 평가된다.
class ArrayTypeNode : public TypeNode {
public:
  TypeNode::Ptr elementType;
  ExprPtr fixedSize; // nullopt => dynamic / slice
  ArrayTypeNode(Token s, TypeNode::Ptr elem, ExprPtr size)
      : TypeNode(NKind::ARRAY_TYPE, s), elementType(std::move(elem)),
        fixedSize(size) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// 제네릭 타입을 표현하는 AST 노드를 나타낸다.
// 기본 타입 이름과 타입 인자를 보유하며 지원되는 제네릭 종류로 분류된다.
// 지원되지 않는 제네릭은 생성 시점에 진단 오류로 처리된다.
class GenericTypeNode : public TypeNode {
public:
  string baseName;
  vector<TypeNode::Ptr> typeArgs;
  enum class GenericKind {
    HANDLE,
    OPTION,
    RESULT,

  } gKind;
  GenericTypeNode(Token s, const string &base, vector<TypeNode::Ptr> args)
      : TypeNode(NKind::GENERIC_TYPE, s), typeArgs(std::move(args)) {
    if (base == "Handle") {
      gKind = GenericKind::HANDLE;
    }

    else {
      Error::diagnostic(span, "unknwon genertic type : " + s.text);
    }
  }
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};