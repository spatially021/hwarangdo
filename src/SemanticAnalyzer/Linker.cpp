#include "SemanticAnalyzer/Linker.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Symbol.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "util/Error.h"

Linker::Linker(SymbolTable *t) : table(t) {}

void Linker::visit(LiteralExpr *) {}
void Linker::visit(BinaryExpr *) {}
void Linker::visit(VarExpr *) {}
void Linker::visit(UnaryExpr *) {}
void Linker::visit(CallExpr *) {}
void Linker::visit(AssignExpr *) {}
void Linker::visit(MemberExpr *) {}
void Linker::visit(ArrayAccessExpr *) {}
void Linker::visit(TernaryExpr *) {}
void Linker::visit(ThisExpr *) {}
void Linker::visit(SuperExpr *) {}

// Statement Linker::visitor methods
void Linker::visit(ExprStmt *) {}
void Linker::visit(BlockStmt *) {}
void Linker::visit(IfStmt *) {}
void Linker::visit(ForStmt *) {}
void Linker::visit(WhileStmt *) {}
void Linker::visit(SwitchStmt *) {}
void Linker::visit(Case *) {}
void Linker::visit(ReturnStmt *) {}
void Linker::visit(BreakStmt *) {}
void Linker::visit(ContinueStmt *) {}
void Linker::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Linker::visit(EmptyStmt *) {}

// declare Linker::visitor methods
void Linker::visit(ClassDecl *decl) {

  if (decl->baseClass.has_value()) {
    string s = decl->baseClass.value();
    if (table->isType(s)) {
      auto symbol = table->getType(s);
      symbol->decl->isExtended = true;
      if (symbol->kind != TypeSymbol::Kind::CLASS) {
        Error::diagnostic(decl->token, s + " is not class");
      }
    } else {
      Error::diagnostic(decl->token, "unknown parent class '" + s + "'");
    }
  }

  for (auto t : decl->traits) {
    if (!table->isType(t)) {
      Error::diagnostic(decl->token, "unknown trait '" + t + "'");
    }
    auto symbol = table->getType(t);
    if (symbol->kind != TypeSymbol::Kind::TRAIT) {
      Error::diagnostic(decl->token, t + " is not trait");
    }
  }

  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto a : decl->body)
    a->accept(this);
}

void Linker::visit(StructDecl *) {}
void Linker::visit(EnumDecl *) {}
void Linker::visit(ImplDecl *decl) {
  string s = decl->target;
  if (!table->isType(s)) {
    Error::diagnostic(decl->token, "unknown impl target");
  }
  auto symbol = table->getType(s);
  if (symbol->kind != TypeSymbol::Kind::STRUCT) {
    Error::diagnostic(decl->token, s + " is not struct");
  }

  for (auto m : decl->LinkedImplMethods) {
    auto it = symbol->methodsName.find(m->name);
    if (it == symbol->methodsName.end()) {
      symbol->methodsName.emplace(m->name, decl->token);
    } else {
      Error::diagnostic(m->token,
                        "duplicate impl method '" + m->name + "' for struct '" +
                            s + "'",
                        it->second, "previous impl method declared here");
    }
  }
}

void Linker::visit(TraitDecl *) {}

void Linker::visit(FuncDecl *) {}
void Linker::visit(VarDecl *) {}
void Linker::visit(ArrayDecl *) {}

void Linker::visit(TypeNode *) {}
void Linker::visit(ASTNode *) {}

void Linker::visit(TraitSig *) {}
void Linker::visit(Param *) {}