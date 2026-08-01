#include "hrd/SemanticAnalyzer/Verifier.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/symbol//MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol//TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/enums/InheritState.h"
#include "hrd/util/Error.h"
#include <variant>

void Verifier::verify() {
  InheritState i = InheritState::Unvisited;
  for (auto &s : program->sources) {
    for (auto &d : s->decls) {
      if (auto c = dynamic_cast<ClassDecl *>(d.get())) {
        inheritStates.emplace(c, i);
      }
      d->accept(this);
    }
  }

  for (auto &d : inheritStates) {
    if (d.second == InheritState::Unvisited) {
      verifyCycledInherit(d.first);
    }
  }
}

void Verifier::verifyCycledInherit(ClassDecl *decl) {
  if (inheritStates[decl] == InheritState::Done)
    return;

  if (inheritStates[decl] == InheritState::Visiting) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S118);
    dia.labels = {
        {decl->span, "inheritance cycle detected here", true},
    };
    dia.notes = {
        "a class cannot inherit from itself directly or indirectly",
    };
    dia.helps = {
        "remove the cyclic inheritance relationship",
    };
    engine.emit(dia);
    recover.recover();
  }

  inheritStates[decl] = InheritState::Visiting;

  if (decl->baseClass.has_value()) {
    if (auto c = dynamic_cast<ClassDecl *>(decl->symbol->base->decl)) {
      verifyCycledInherit(c);
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S119);
      dia.labels = {
          {decl->span, "base type is not a class", true},
      };
      dia.helps = {
          "inherit only from class types",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  inheritStates[decl] = InheritState::Done;
}

void Verifier::visit(LiteralExpr *expr) {
  if (expr->resolvedType == nullptr)
    unresolved(expr, "literalExpr is unresovled");
}
void Verifier::visit(BinaryExpr *expr) {

  expr->left->accept(this);
  expr->right->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "binaryExpr is unresolved");
}
void Verifier::visit(NameExpr *expr) {
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "nameExpr is unresolved");
  }

  if (expr->resolvedType == nullptr)
    unresolved(expr, "varExpr's type is unresolved");
}
void Verifier::visit(UnaryExpr *expr) {
  expr->right->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "unaryExpr is unresolved");
}
void Verifier::visit(CallExpr *expr) {
  for (auto &a : expr->arguments) {
    a->accept(this);
  }

  if (expr->receiver != nullptr &&
      get_if<RuntimeSymbol *>(&expr->resolved) == nullptr) {
    expr->receiver->accept(this);
  }

  if (expr->callType == CallExpr::CallType::FUNC_CALL) {
    if (get_if<MethodSymbol *>(&expr->resolved) == nullptr)
      unresolved(expr, "method is unresolved");
  } else if (expr->callType == CallExpr::CallType::PAYLOAD_CALL) {
    if (get_if<EnumVariantSymbol *>(&expr->resolved) == nullptr)
      unresolved(expr, "variant is unresolved");
  } else if (expr->callType == CallExpr::CallType::INIT_CALL) {
    if (expr->resolvedType == nullptr) {
      unresolved(expr, "init is unresolved");
    }
  } else if (expr->callType == CallExpr::CallType::RUNTIME_CALL) {
    if (get_if<RuntimeSymbol *>(&expr->resolved) == nullptr) {
      unresolved(expr, "runtimeCall is unresolved");
    }
  } else {
    unresolved(expr, "callExpr unresolved");
  }

  if (expr->resolvedType == nullptr) {
    unresolved(expr, "callExpr's resolvedType is unresolved");
  }
}
void Verifier::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "assignExpr is unresolved");
}
void Verifier::visit(MemberExpr *expr) {
  expr->object->accept(this);
  if (expr->resolved == nullptr)
    unresolved(expr, "memberExpr is unresolved");
}
void Verifier::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "arrayAccessExpr is unresolved");
}
void Verifier::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->else_->accept(this);
  expr->then->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "ternarExpr is unresolved");
}
void Verifier::visit(ThisExpr *) {}
void Verifier::visit(SuperExpr *) {}
void Verifier::visit(RootExpr *) {}
void Verifier::visit(SelfExpr *) {}
void Verifier::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "castExpr is unresolved");
  }
}
void Verifier::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  expr->spawnType->accept(this);
  for (auto &p : expr->args) {
    p->accept(this);
  }
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "spawnExpr is unresolved");
  }
}
void Verifier::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "viewExpr is unresolved");
  }
}

void Verifier::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  expr->target->accept(this);
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "destroyExpr is unresolved");
  }
}

void Verifier::visit(QuitExpr *) {}
void Verifier::visit(DefaultValueExpr *) {}
void Verifier::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
  if (expr->step) {
    expr->step->accept(this);
  }
}
void Verifier::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Verifier::visit(MatchExpr *expr) {
  expr->value->accept(this);
  for (auto &c : expr->cases) {
    c->accept(this);
  }
  if (!expr->resolvedType) {
    unresolved(expr, "unresolved match type");
  }
}
// Statement Verifier::visitor methods
void Verifier::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Verifier::visit(BlockStmt *stmt) {
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
}
void Verifier::visit(BuiltInNameExpr *) {}

void Verifier::visit(IfStmt *stmt) {
  stmt->condition->accept(this);
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch) {
    stmt->elseBranch->accept(this);
  }
}
void Verifier::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  context.loopDepth++;
  stmt->body->accept(this);
  context.loopDepth--;
}
void Verifier::visit(WhileStmt *stmt) {
  stmt->condition->accept(this);
  context.loopDepth++;
  stmt->body->accept(this);
  context.loopDepth--;
}
void Verifier::visit(SwitchStmt *stmt) {
  stmt->value->accept(this);
  for (auto c : stmt->clauses) {
    c->accept(this);
  }
}
void Verifier::visit(Case *stmt) { stmt->body->accept(this); }
void Verifier::visit(ReturnStmt *stmt) {
  if (stmt->value != nullptr) {
    if (stmt->returnType == nullptr)
      unresolved(stmt, "returnStmt is unresolved");
  }
}
void Verifier::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Verifier::visit(BreakStmt *stmt) {
  if (context.loopDepth == 0) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S120);
    dia.labels = {
        {stmt->span, "break is not inside a loop", true},
    };
    dia.helps = {
        "move the break statement into a loop",
    };
    engine.emit(dia);
  }
}
void Verifier::visit(ContinueStmt *stmt) {
  if (context.loopDepth == 0) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S121);
    dia.labels = {
        {stmt->span, "continue is not inside a loop", true},
    };
    dia.helps = {
        "move the continue statement into a loop",
    };
    engine.emit(dia);
  }
}
void Verifier::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Verifier::visit(EmptyStmt *) {}

// declare Verifier::visitor methods
void Verifier::visit(ClassDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "ClassDecl is unresolved");
  for (auto &a : decl->fields) {
    a->accept(this);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
}
void Verifier::visit(StructDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "structDecl is unresolved");
  for (auto &f : decl->fields) {
    f->accept(this);
  }
}
void Verifier::visit(EnumDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "EnumDecl is unresolved");
}
void Verifier::visit(ImplDecl *decl) {
  for (auto &m : decl->LinkedImplMethods) {
    m->accept(this);
  }
}
void Verifier::visit(TraitDecl *decl) {
  for (auto &s : decl->traitSigs) {
    s->accept(this);
  }
}
void Verifier::visit(TraitSig *sig) {
  if (sig->symbol == nullptr)
    unresolved(sig, "trait signiture is unresolved");
  for (auto &p : sig->params) {
    p->accept(this);
  }
  if (sig->symbol->returnType == nullptr) {
    unresolved(sig, "trait signiture's returnType is unresolved");
  }
}
void Verifier::visit(FuncDecl *decl) {
  if (decl->methodSymbol == nullptr)
    unresolved(decl, "funcDecl is unresolved");
  if (decl->methodSymbol->returnType == nullptr) {
    unresolved(decl, "funcDecl's retrunType is nullptr");
  }
  for (auto &p : decl->params)
    p->accept(this);
  decl->body->accept(this);
}
void Verifier::visit(VarDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "varDecl is unresolved");
}

void Verifier::visit(TypeNode *) {}
void Verifier::visit(ASTNode *) {}
void Verifier::visit(Param *param) {
  if (param->symbol == nullptr)
    unresolved(param, "param is unresolved");
}

void Verifier::visit(InitDecl *decl) {
  if (decl->methodSymbol == nullptr) {
    unresolved(decl, "init is unresolved");
  }
  for (auto &p : decl->params)
    p->accept(this);
  decl->body->accept(this);
}

void Verifier::visit(OnDestroyDecl *decl) {
  if (decl->methodSymbol == nullptr) {
    unresolved(decl, "onDestroy is unresolved");
  }
  decl->body->accept(this);
}

void Verifier::unresolved(ASTNode *node, const string &msg) {
  Error::internal(node->span, msg);
}
