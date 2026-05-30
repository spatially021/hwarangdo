#pragma once

#include "AST/CaseKey.h"
#include "ASTNode.h"
#include "SourceSpan.h"
#include "Token.h"
#include "Visitor.h"
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

class Decl;
class Expr;
class Scope;
class Range;
class HIRStmt;
using DeclPtr = shared_ptr<Decl>;
using ExprPtr = shared_ptr<Expr>;

class Stmt : public ASTNode {
public:
  using Ptr = std::shared_ptr<Stmt>;
  Stmt(NKind k, SourceSpan t) : ASTNode(k, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  Scope *blockScope = nullptr;
};

class ExprStmt : public Stmt {
public:
  ExprPtr expr;

  ExprStmt(SourceSpan t, ExprPtr e)
      : Stmt(NKind::EXPR_STMT, t), expr(std::move(e)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BlockStmt : public Stmt {
public:
  std::vector<Stmt::Ptr> statements;

  BlockStmt(SourceSpan t, std::vector<Stmt::Ptr> stmts)
      : Stmt(NKind::BLOCK_STMT, t), statements(std::move(stmts)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class IfStmt : public Stmt {
public:
  ExprPtr condition;
  Stmt::Ptr thenBranch;
  Stmt::Ptr elseBranch;

  IfStmt(SourceSpan t, ExprPtr cond, Stmt::Ptr thenB, Stmt::Ptr elseB = nullptr)
      : Stmt(NKind::IF_STMT, t), condition(std::move(cond)),
        thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class WhileStmt : public Stmt {
public:
  ExprPtr condition;
  Stmt::Ptr body;

  WhileStmt(SourceSpan t, ExprPtr c, Stmt::Ptr b)
      : Stmt(NKind::WHILE_STMT, t), condition(std::move(c)),
        body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ForStmt : public Stmt {
public:
  shared_ptr<DeclStmt> initializer; // VarDeclStmt or ExprStmt or null
  shared_ptr<Range> range;
  Stmt::Ptr body;

  ForStmt(SourceSpan t, shared_ptr<DeclStmt> init, shared_ptr<Range> r,
          Stmt::Ptr b)
      : Stmt(NKind::FOR_STMT, t), initializer(std::move(init)),
        range(std::move(r)), body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ReturnStmt : public Stmt {
public:
  ExprPtr value = nullptr; // null이면 return;
  ReturnStmt(SourceSpan t, ExprPtr v)
      : Stmt(NKind::RETURN_STMT, t), value(std::move(v)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *returnType = nullptr;
};

class ValueTransferStmt : public Stmt {
public:
  ExprPtr value = nullptr;
  ValueTransferStmt(SourceSpan t, ExprPtr v)
      : Stmt(NKind::VALUE_TRANSFER_STMT, t), value(std::move(v)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *returnType = nullptr;
};

class BreakStmt : public Stmt {
public:
  Token token;
  BreakStmt(SourceSpan s, Token t) : Stmt(NKind::BREAK_STMT, s), token(t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ContinueStmt : public Stmt {
public:
  Token token;
  ContinueStmt(SourceSpan t, Token tk)
      : Stmt(NKind::CONTINUE_STMT, t), token(tk) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class Case : public ASTNode {
public:
  vector<ExprPtr> values;
  Stmt::Ptr body;
  bool isDefault = false;
  bool isWildCard = false;
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  Case(SourceSpan t, vector<ExprPtr> v, Stmt::Ptr b, bool isd = false,
       bool isw = false)
      : ASTNode(NKind::SWITCH_CASE, t), values(std::move(v)), body(b),
        isDefault(isd), isWildCard(isw) {}
  vector<ExprPtr> transfers;
  TypeSymbol *transferType = nullptr;
};

class SwitchStmt : public Stmt {
public:
  ExprPtr value;                         // switch (value)
  std::vector<shared_ptr<Case>> clauses; // CaseStmt 또는 DefaultStmt 의 집합

  bool hasDefault = false;

  SwitchStmt(SourceSpan t, ExprPtr val, std::vector<shared_ptr<Case>> c)
      : Stmt(NKind::SWITCH_STMT, t), value(std::move(val)),
        clauses(std::move(c)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  unordered_set<CaseKey, CaseKeyHash> caseKeys;
  std::unordered_set<EnumVariantSymbol *> usedVariants;
};

class CatchClause : public Stmt {
public:
  std::shared_ptr<TypeNode> type;      // catch (e: ErrorType)
  optional<std::string> exceptionName; // catch (e)
  Stmt::Ptr body;                      // block or single stmt

  CatchClause(SourceSpan tok, std::shared_ptr<TypeNode> t,
              optional<std::string> name, Stmt::Ptr b)
      : Stmt(NKind::CATCH_STMT, tok), type(std::move(t)), exceptionName(name),
        body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class TryCatchStmt : public Stmt {
public:
  Stmt::Ptr tryBlock; // usually BlockStmt
  std::vector<std::shared_ptr<CatchClause>> catches;

  TryCatchStmt(SourceSpan t, Stmt::Ptr tryB,
               std::vector<std::shared_ptr<CatchClause>> c)
      : Stmt(NKind::TRY_STMT, t), tryBlock(std::move(tryB)),
        catches(std::move(c)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class OnexitStmt : public Stmt {
public:
  Stmt::Ptr body;

  OnexitStmt(SourceSpan t, Stmt::Ptr b)
      : Stmt(NKind::ONEXIT_STMT, t), body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class EmptyStmt : public Stmt {
public:
  EmptyStmt(SourceSpan t) : Stmt(NKind::EMPTY_STMT, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class DeclStmt : public Stmt {
public:
  DeclPtr decl;
  DeclStmt(SourceSpan t, DeclPtr d) : Stmt(NKind::DECL_STMT, t), decl(d) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ThrowStmt : public Stmt {
public:
  ExprPtr expr;
  ThrowStmt(SourceSpan t, ExprPtr ex) : Stmt(NKind::THROW_STMT, t), expr(ex) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};