#pragma once

#include "ASTNode.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SourceSpan.h"
#include "Token.h"
#include "Visitor.h"
#include "enums/Operator.h"
#include "util/Error.h"
#include <memory>
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
  Expr(NKind k, SourceSpan t) : ASTNode(k, t) {}
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
  Token token;
  LiteralExpr(SourceSpan t, Token tok, const string &v)
      : Expr(NKind::LITERAL_EXPR, t), value(v), token(tok) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<LiteralExpr>(span, token, value);
  }

  ResolvedLit resolvedLit;
};

class NameExpr : public Expr {
public:
  string name;
  NameExpr(SourceSpan t, const string &n)
      : Expr(NKind::NAME_EXPR, t), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<NameExpr>(span, name); }

  Symbol *resolved = nullptr;
};

class UnaryExpr : public Expr {
public:
  Token tOp;
  Operator op;
  Ptr right;
  UnaryExpr(SourceSpan t, Token o, Ptr p)
      : Expr(NKind::UNARY_EXPR, t), tOp(o), right(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<UnaryExpr>(span, tOp,
                                  right ? right->deepCopy() : nullptr);
  }
};

class BinaryExpr : public Expr {
public:
  Ptr left, right;
  Operator op;
  Token opRaw;

  BinaryExpr(SourceSpan t, Ptr l, Token o, Ptr r)
      : Expr(NKind::BINARY_EXPR, t), left(std::move(l)), right(std::move(r)),
        opRaw(o) {
    switch (o.kind) {
    case TKind::DOUBLE_EQUAL:
      op = Operator::EQ;
      break;
    case TKind::BANG_EQUAL:
      op = Operator::NT;
      break;
    case TKind::LESS:
      op = Operator::LS;
      break;
    case TKind::GREATER:
      op = Operator::GR;
      break;
    case TKind::LESS_EQUAL:
      op = Operator::LSE;
      break;
    case TKind::GREATER_EQUAL:
      op = Operator::GRE;
      break;
    case TKind::AND:
      op = Operator::AND;
      break;
    case TKind::OR:
      op = Operator::OR;
      break;
    case TKind::DOUBLE_ANGLEBUCKET:
      op = Operator::LSH;
      break;
    case TKind::CARET:
      op = Operator::B_XOR;
      break;
    case TKind::AMPERSAND:
      op = Operator::B_AND;
      break;
    case TKind::PIPE:
      op = Operator::B_OR;
      break;
    case TKind::DOUBLE_RIGHT_ANGLE_BUCKET:
      op = Operator::RSH;
      break;
    case TKind::PLUS:
      op = Operator::ADD;
      break;
    case TKind::MINUS:
      op = Operator::SUB;
      break;
    case TKind::SLASH:
      op = Operator::DIV;
      break;
    case TKind::PERCENT:
      op = Operator::REM;
      break;
    case TKind::STAR:
      op = Operator::MUL;
      break;
    case TKind::DOUBLE_STAR:
      op = Operator::POW;
      break;
    default:
      Error::diagnostic(span, "unexpected Token kind in binary "
                              "operator");
    }
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<BinaryExpr>(span, left ? left->deepCopy() : nullptr,
                                   opRaw, right ? right->deepCopy() : nullptr);
  }
};

class AssignExpr : public Expr {
public:
  Expr::Ptr target;
  Expr::Ptr value;
  Token op;

  AssignExpr(SourceSpan tok, Expr::Ptr t, Token o, Expr::Ptr v)
      : Expr(NKind::ASSIGN_EXPR, tok), target(std::move(t)),
        value(std::move(v)), op(o) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<AssignExpr>(span, target ? target->deepCopy() : nullptr,
                                   op, value ? value->deepCopy() : nullptr);
  }
};

class MemberExpr : public Expr {
public:
  Expr::Ptr object;
  string member;
  MemberExpr(SourceSpan t, Expr::Ptr o, const std::string &m)
      : Expr(NKind::MEMBER_EXPR, t), object(std::move(o)), member(m) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<MemberExpr>(span, object ? object->deepCopy() : nullptr,
                                   member);
  }

  ValueSymbol *resolved = nullptr;
};

class ArrayAccessExpr : public Expr {
public:
  Expr::Ptr object;
  Expr::Ptr index;

  ArrayAccessExpr(SourceSpan t, Expr::Ptr o, Expr::Ptr i)
      : Expr(NKind::ARRAY_ACCESS_EXPR, t), object(std::move(o)),
        index(std::move(i)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<ArrayAccessExpr>(span,
                                        object ? object->deepCopy() : nullptr,
                                        index ? index->deepCopy() : nullptr);
  }
};

class CallExpr : public Expr {
public:
  Expr::Ptr receiver = nullptr;
  string methodName;
  std::vector<Expr::Ptr> arguments;

  enum class CallType {
    FUNC_CALL,
    PAYLOAD_CALL,
    BUILTIN_CALL,
    INIT_CALL,
    UNRESOLVED,
  } callType = CallExpr::CallType::UNRESOLVED;

  CallExpr(SourceSpan t, Expr::Ptr r, const string n,
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

    return make_shared<CallExpr>(span,
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

  TernaryExpr(SourceSpan t, Expr::Ptr c, Expr::Ptr th, Expr::Ptr e)
      : Expr(NKind::TERNARY_EXPR, t), conditon(std::move(c)),
        then(std::move(th)), else_(std::move(e)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<TernaryExpr>(
        span, conditon ? conditon->deepCopy() : nullptr,
        then ? then->deepCopy() : nullptr, else_ ? else_->deepCopy() : nullptr);
  }
};

class NewExpr : public Expr {
public:
  std::string typeName;
  std::vector<Expr::Ptr> args;

  NewExpr(SourceSpan t, const std::string &ty, std::vector<Expr::Ptr> a)
      : Expr(NKind::NEW_EXPR, t), typeName(ty), args(std::move(a)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    vector<Expr::Ptr> copiedArgs;
    copiedArgs.reserve(args.size());
    for (const auto &arg : args) {
      copiedArgs.push_back(arg ? arg->deepCopy() : nullptr);
    }

    return make_shared<NewExpr>(span, typeName, std::move(copiedArgs));
  }
};

class ThisExpr : public Expr {
public:
  ThisExpr(SourceSpan t) : Expr(NKind::THIS_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<ThisExpr>(span); }

  TypeSymbol *resolved = nullptr;
};

class SuperExpr : public Expr {
public:
  SuperExpr(SourceSpan t) : Expr(NKind::SUPER_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<SuperExpr>(span); }

  TypeSymbol *resolved = nullptr;
};

class SelfExpr : public Expr {
public:
  SelfExpr(SourceSpan t) : Expr(NKind::SELF_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<SelfExpr>(span); }

  TypeSymbol *resolved = nullptr;
};

class RootExpr : public Expr {
public:
  RootExpr(SourceSpan t) : Expr(NKind::ROOT_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<RootExpr>(span); }

  TypeSymbol *resolved = nullptr;
};

class MatchExpr : public Expr {
public:
  Ptr value;
  vector<shared_ptr<Case>> cases;
  MatchExpr(SourceSpan t, Ptr v, vector<shared_ptr<Case>> c)
      : Expr(NKind::MATCH_EXPR, t), value(std::move(v)), cases(std::move(c)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  Scope *blockScope = nullptr;
  Ptr deepCopy() const override {
    vector<shared_ptr<Case>> copiedCases;
    copiedCases.reserve(cases.size());
    return make_shared<MatchExpr>(span, value ? value->deepCopy() : nullptr,
                                  std::move(copiedCases));
  }
};

class EnumVariantExpr : public Expr {
public:
  string name;
  Expr::Ptr receiver;
  Expr::Ptr payload;
  EnumVariantExpr(SourceSpan t, string const &n, Expr::Ptr r, Expr::Ptr p)
      : Expr(NKind::ENUM_VARIANT_EXPR, t), name(n), receiver(std::move(r)),
        payload(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<EnumVariantExpr>(
        span, name, receiver ? receiver->deepCopy() : nullptr,
        payload ? payload->deepCopy() : nullptr);
  }
};

class Range : public Expr {
public:
  ExprPtr from;
  ExprPtr to;
  ExprPtr step = nullptr;
  Range(SourceSpan t, ExprPtr f, ExprPtr to_, ExprPtr s = nullptr)
      : Expr(NKind::RANGE, t), from(std::move(f)), to(std::move(to_)),
        step(std::move(s)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<Range>(span, from ? from->deepCopy() : nullptr,
                              to ? to->deepCopy() : nullptr,
                              step ? step->deepCopy() : nullptr);
  }
};

class CastExpr : public Expr {
public:
  ExprPtr left = nullptr;
  shared_ptr<TypeNode> type = nullptr;
  CastExpr(SourceSpan t, ExprPtr l, shared_ptr<TypeNode> ty)
      : Expr(NKind::CAST_EXPR, t), left(std::move(l)), type(std::move(ty)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<CastExpr>(span, left ? left->deepCopy() : nullptr, type);
  }
};

class BuiltInNameExpr : public Expr {
public:
  string name;
  Token token;
  enum class StorageType {
    WORLD,
    ARENA,

  } storageType;
  BuiltInNameExpr(SourceSpan t, Token tok, string n)
      : Expr(NKind::BUILTIN_NAME_EXPR, t), name(std::move(n)), token(tok) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    auto copied = make_shared<BuiltInNameExpr>(span, token, name);
    copied->storageType = storageType;
    return copied;
  }
};

class SpawnExpr : public Expr {
public:
  ExprPtr left = nullptr;
  TypeNode::Ptr spawnType = nullptr;
  vector<ExprPtr> args;
  SpawnExpr(SourceSpan t, ExprPtr l, TypeNode::Ptr s, vector<ExprPtr> a)
      : Expr(NKind::SPAWN_EXPR, t), left(std::move(l)), spawnType(std::move(s)),
        args(std::move(a)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    vector<ExprPtr> copiedArgs;
    copiedArgs.reserve(args.size());
    for (const auto &arg : args) {
      copiedArgs.push_back(arg ? arg->deepCopy() : nullptr);
    }

    return make_shared<SpawnExpr>(span, left ? left->deepCopy() : nullptr,
                                  spawnType, std::move(copiedArgs));
  }

  MethodSymbol *resolvedInit = nullptr;
};

class ViewExpr : public Expr {
public:
  ExprPtr left = nullptr;
  ExprPtr target = nullptr;
  ViewExpr(SourceSpan t, ExprPtr l, ExprPtr tar)
      : Expr(NKind::VIEW_EXPR, t), left(std::move(l)), target(std::move(tar)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<ViewExpr>(span, left ? left->deepCopy() : nullptr,
                                 target ? target->deepCopy() : nullptr);
  }
};

class DestroyExpr : public Expr {
public:
  ExprPtr storage = nullptr;
  ExprPtr target = nullptr;
  DestroyExpr(SourceSpan t, ExprPtr s, ExprPtr tg)
      : Expr(NKind::DESTROY_EXPR, t), storage(std::move(s)),
        target(std::move(tg)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override {
    return make_shared<DestroyExpr>(span, storage, target);
  }
};

class DefaultValueExpr : public Expr {
public:
  DefaultValueExpr(SourceSpan t) : Expr(NKind::DEFUALT_VALUE_EXPR, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  Ptr deepCopy() const override { return make_shared<DefaultValueExpr>(span); }

  ValueSymbol *resolve = nullptr;
};

class CaseValueExpr : public Expr {
public:
  Ptr value = nullptr;
  Ptr arg = nullptr;
  CaseValueExpr(SourceSpan t, Ptr v, Ptr a)
      : Expr(NKind::VALUE_EXPR, t), value(v), arg(a) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  Ptr deepCopy() const override {
    return make_shared<CaseValueExpr>(span, value, arg);
  }
  ValueSymbol *variant = nullptr;
  TypeSymbol *payloadType = nullptr;
};