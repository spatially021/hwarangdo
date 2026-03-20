#pragma once

#include "ASTNode.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "Token.h"
#include "Visitor.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class ValueSymbol;
class TypeSymbol;
class MethodSymbol;
class EnumVariantSymbol;
class Symbol;
class Stmt;
class TypeNode;
class Scope;

using namespace std;

using StmtPtr = shared_ptr<Stmt>;
class Expr : public ASTNode {
public:
  using Ptr = shared_ptr<Expr>;
  Expr(NKind k, Token t) : ASTNode(k, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  virtual Ptr deepCopy() const = 0;

  TypeSymbol *resolvedType = nullptr;
  enum class State {
    RESOLVED,
    NEED_CHECK,
    UNKNOWN,
  } state = Expr::State::RESOLVED;
};

class LiteralExpr : public Expr {
public:
  string value = "null";
  LiteralExpr(Token t, const string &v)
      : Expr(NKind::LITERAL_EXPR, t), value(v) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<LiteralExpr>(token, value);
  }

  ResolvedLit resolvedLit;
};

class NameExpr : public Expr {
public:
  string name;
  NameExpr(Token t, const string &n) : Expr(NKind::NAME_EXPR, t), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<NameExpr>(token, name); }

  TypeSymbol *typeSymbol = nullptr;
  ValueSymbol *valueSymbol = nullptr;
};

class UnaryExpr : public Expr {
public:
  Token op;
  Ptr right;
  UnaryExpr(Token t, Token o, Ptr p)
      : Expr(NKind::UNARY_EXPR, t), op(o), right(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<UnaryExpr>(token, op,
                                  right ? right->deepCopy() : nullptr);
  }
};

class BinaryExpr : public Expr {
public:
  Ptr left, right;
  enum class OperatorType {
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    POW,

    B_AND,
    B_OR,
    B_XOR,

    AND,
    OR,

    EQ,
    NT,
    LS,
    LSE,
    GR,
    GRE,

    LSH,
    RSH,

  } op;
  Token opRaw;

  BinaryExpr(Token t, Ptr l, Token o, Ptr r)
      : Expr(NKind::BINARY_EXPR, t), left(std::move(l)), right(std::move(r)),
        opRaw(o) {
    switch (o.kind) {
    case TKind::DOUBLE_EQUAL:
      op = OperatorType::EQ;
      break;
    case TKind::BANG_EQUAL:
      op = OperatorType::NT;
      break;
    case TKind::LESS:
      op = OperatorType::LS;
      break;
    case TKind::GREATER:
      op = OperatorType::GR;
      break;
    case TKind::LESS_EQUAL:
      op = OperatorType::LSE;
      break;
    case TKind::GREATER_EQUAL:
      op = OperatorType::GRE;
      break;
    case TKind::AND:
      op = OperatorType::AND;
      break;
    case TKind::OR:
      op = OperatorType::OR;
      break;
    case TKind::DOUBLE_ANGLEBUCKET:
      op = OperatorType::LSH;
      break;
    case TKind::CARET:
      op = OperatorType::B_XOR;
      break;
    case TKind::AMPERSAND:
      op = OperatorType::B_AND;
      break;
    case TKind::PIPE:
      op = OperatorType::B_OR;
      break;
    case TKind::DOUBLE_RIGHT_ANGLE_BUCKET:
      op = OperatorType::RSH;
      break;
    case TKind::PLUS:
      op = OperatorType::ADD;
      break;
    case TKind::MINUS:
      op = OperatorType::SUB;
      break;
    case TKind::SLASH:
      op = OperatorType::DIV;
      break;
    case TKind::PERCENT:
      op = OperatorType::REM;
      break;
    case TKind::STAR:
      op = OperatorType::MUL;
      break;
    case TKind::DOUBLE_STAR:
      op = OperatorType::POW;
      break;
    default:
      throw runtime_error("[line : " + to_string(o.line) +
                          ", col : " + to_string(o.col) +
                          "] unexpected token kind in binary operator");
    }
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<BinaryExpr>(token, left ? left->deepCopy() : nullptr,
                                   opRaw, right ? right->deepCopy() : nullptr);
  }
};

class AssignExpr : public Expr {
public:
  Expr::Ptr target;
  Expr::Ptr value;
  Token op;

  AssignExpr(Token tok, Expr::Ptr t, Token o, Expr::Ptr v)
      : Expr(NKind::ASSIGN_EXPR, tok), target(std::move(t)),
        value(std::move(v)), op(o) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<AssignExpr>(token, target ? target->deepCopy() : nullptr,
                                   op, value ? value->deepCopy() : nullptr);
  }
};

class MemberExpr : public Expr {
public:
  Expr::Ptr object;
  string member;
  MemberExpr(Token t, Expr::Ptr o, const std::string &m)
      : Expr(NKind::MEMBER_EXPR, t), object(std::move(o)), member(m) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<MemberExpr>(token, object ? object->deepCopy() : nullptr,
                                   member);
  }

  ValueSymbol *resolved = nullptr;
};

class ArrayAccessExpr : public Expr {
public:
  Expr::Ptr object;
  Expr::Ptr index;

  ArrayAccessExpr(Token t, Expr::Ptr o, Expr::Ptr i)
      : Expr(NKind::ARRAY_ACCESS_EXPR, t), object(std::move(o)),
        index(std::move(i)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<ArrayAccessExpr>(token,
                                        object ? object->deepCopy() : nullptr,
                                        index ? index->deepCopy() : nullptr);
  }
};

class CallExpr : public Expr {
public:
  Expr::Ptr receiver;
  string methodName;
  std::vector<Expr::Ptr> arguments;

  enum class CallType {
    FUNC_CALL,
    PAYLOAD_CALL,
    BUILTIN_CALL,
    UNRESOLVED,
  } callType = CallExpr::CallType::UNRESOLVED;

  CallExpr(Token t, Expr::Ptr r, const string n,
           const std::vector<Expr::Ptr> &a)
      : Expr(NKind::CALL_EXPR, t), receiver(std::move(r)), methodName(n),
        arguments(a) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    vector<Expr::Ptr> copiedArgs;
    copiedArgs.reserve(arguments.size());
    for (const auto &arg : arguments) {
      copiedArgs.push_back(arg ? arg->deepCopy() : nullptr);
    }

    return make_shared<CallExpr>(token,
                                 receiver ? receiver->deepCopy() : nullptr,
                                 methodName, copiedArgs);
  }

  Symbol *resolved = nullptr;
};

class TernaryExpr : public Expr {
public:
  Expr::Ptr conditon;
  Expr::Ptr then;
  Expr::Ptr else_;

  TernaryExpr(Token t, Expr::Ptr c, Expr::Ptr th, Expr::Ptr e)
      : Expr(NKind::TERNARY_EXPR, t), conditon(std::move(c)),
        then(std::move(th)), else_(std::move(e)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<TernaryExpr>(
        token, conditon ? conditon->deepCopy() : nullptr,
        then ? then->deepCopy() : nullptr, else_ ? else_->deepCopy() : nullptr);
  }
};

class NewExpr : public Expr {
public:
  std::string typeName;
  std::vector<Expr::Ptr> args;

  NewExpr(Token t, const std::string &ty, std::vector<Expr::Ptr> a)
      : Expr(NKind::NEW_EXPR, t), typeName(ty), args(std::move(a)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    vector<Expr::Ptr> copiedArgs;
    copiedArgs.reserve(args.size());
    for (const auto &arg : args) {
      copiedArgs.push_back(arg ? arg->deepCopy() : nullptr);
    }

    return make_shared<NewExpr>(token, typeName, std::move(copiedArgs));
  }
};

class ThisExpr : public Expr {
public:
  ThisExpr(Token t) : Expr(NKind::THIS_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<ThisExpr>(token); }

  TypeSymbol *resolved = nullptr;
};

class SuperExpr : public Expr {
public:
  SuperExpr(Token t) : Expr(NKind::SUPER_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<SuperExpr>(token); }

  TypeSymbol *resolved = nullptr;
};

class MatchExpr : public Expr {
public:
  Ptr value;
  vector<shared_ptr<Case>> cases;
  MatchExpr(Token t, Ptr v, vector<shared_ptr<Case>> c)
      : Expr(NKind::MATCH_EXPR, t), value(std::move(v)), cases(std::move(c)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  Scope *blockScope = nullptr;
  Ptr deepCopy() const override {
    vector<shared_ptr<Case>> copiedCases;
    copiedCases.reserve(cases.size());
    return make_shared<MatchExpr>(token, value ? value->deepCopy() : nullptr,
                                  std::move(copiedCases));
  }
};

class EnumVariantExpr : public Expr {
public:
  string name;
  Expr::Ptr receiver;
  Expr::Ptr payload;
  EnumVariantExpr(Token t, string const &n, Expr::Ptr r, Expr::Ptr p)
      : Expr(NKind::ENUM_VARIANT_EXPR, t), name(n), receiver(std::move(r)),
        payload(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<EnumVariantExpr>(
        token, name, receiver ? receiver->deepCopy() : nullptr,
        payload ? payload->deepCopy() : nullptr);
  }
};

class Range : public Expr {
public:
  ExprPtr from;
  ExprPtr to;
  ExprPtr step = nullptr;
  Range(Token t, ExprPtr f, ExprPtr to_, ExprPtr s = nullptr)
      : Expr(NKind::RANGE, t), from(std::move(f)), to(std::move(to_)),
        step(std::move(s)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<Range>(token, from ? from->deepCopy() : nullptr,
                              to ? to->deepCopy() : nullptr,
                              step ? step->deepCopy() : nullptr);
  }
};

class CastExpr : public Expr {
public:
  ExprPtr left = nullptr;
  shared_ptr<TypeNode> type = nullptr;
  CastExpr(Token t, ExprPtr l, shared_ptr<TypeNode> ty)
      : Expr(NKind::CAST_EXPR, t), left(std::move(l)), type(std::move(ty)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<CastExpr>(token, left ? left->deepCopy() : nullptr,
                                 type);
  }
};

class BuiltInNameExpr : public Expr {
public:
  string name;
  enum class StorageType {
    WORLD,
    ARENA,

  } storageType;
  BuiltInNameExpr(Token t, string n)
      : Expr(NKind::BUILTIN_NAME_EXPR, t), name(std::move(n)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    auto copied = make_shared<BuiltInNameExpr>(token, name);
    copied->storageType = storageType;
    return copied;
  }
};

class SpawnExpr : public Expr {
public:
  ExprPtr left = nullptr;
  TypeNode::Ptr spawnType = nullptr;
  vector<ExprPtr> args;
  SpawnExpr(Token t, ExprPtr l, TypeNode::Ptr s, vector<ExprPtr> a)
      : Expr(NKind::SPAWN_EXPR, t), left(std::move(l)), spawnType(std::move(s)),
        args(std::move(a)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    vector<ExprPtr> copiedArgs;
    copiedArgs.reserve(args.size());
    for (const auto &arg : args) {
      copiedArgs.push_back(arg ? arg->deepCopy() : nullptr);
    }

    return make_shared<SpawnExpr>(token, left ? left->deepCopy() : nullptr,
                                  spawnType, std::move(copiedArgs));
  }
};

class ViewExpr : public Expr {
public:
  ExprPtr left = nullptr;
  ExprPtr target = nullptr;
  ViewExpr(Token t, ExprPtr l, ExprPtr tar)
      : Expr(NKind::VIEW_EXPR, t), left(std::move(l)), target(std::move(tar)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<ViewExpr>(token, left ? left->deepCopy() : nullptr,
                                 target ? target->deepCopy() : nullptr);
  }
};

class DefaultValueExpr : public Expr {
public:
  DefaultValueExpr(Token t) : Expr(NKind::DEFUALT_VALUE_EXPR, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<DefaultValueExpr>(token); }

  ValueSymbol *resolve = nullptr;
};

class CaseValueExpr : public Expr {
public:
  Ptr value = nullptr;
  Ptr arg = nullptr;
  CaseValueExpr(Token t, Ptr v, Ptr a)
      : Expr(NKind::VALUE_EXPR, t), value(v), arg(a) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  Ptr deepCopy() const override {
    return make_shared<CaseValueExpr>(token, value, arg);
  }
  ValueSymbol *variant = nullptr;
  TypeSymbol *payloadType = nullptr;
};