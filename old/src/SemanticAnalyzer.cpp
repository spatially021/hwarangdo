#include "include/SemanticAnalyzer.h"
#include "include/AST/Decl.h"
#include "include/SymbolTable.h"
#include <memory>
#include <stdexcept>

SemanticAnalyzer::SemanticAnalyzer(shared_ptr<Program> program) {
  this->program = program;
}

void SemanticAnalyzer::analyze() { program->accept(this); }
void SemanticAnalyzer::visit(Program *decl) {
  symbols.enterScope();
  for (auto const &m : decl->declarations)
    m->accept(this);
  symbols.exitScope();
}
void SemanticAnalyzer::visit(ClassDecl *decl) {
  // 1) 클래스 심볼 생성 및 등록
  // defineClass에는 기존처럼 name만 넘기고, 내부 storage에 ClassSymbol을
  // 만들도록 했다고 가정 (너의 SymbolTable::define(SymbolKind::CLASS, ...) 기존
  // 호출 방식과 일치하도록 조정)
  if (!symbols.define(SymbolKind::CLASS, SymbolInfo{.name = decl->name})) {
    throw std::runtime_error("Class redefinition: " + decl->name);
  }

  // 심볼테이블에 등록된 ClassSymbol*를 얻어온다
  ClassSymbol *clsSym = symbols.lookupClass(decl->name);
  if (!clsSym) {
    // 정의 직후인데도 못찾으면 내부 구현과 맞지 않는 것.
    throw std::runtime_error(
        "internal error: class symbol not found after define");
  }

  bool oldInClass=insideClass;
  insideClass=true;

  // 2) currentClass 설정 — 이후 VarDecl/FuncDecl 방문 시 멤버로 추가하게 함
  ClassSymbol *prevClass =
      currentClass; // 재귀적/중첩 class 지원을 위해 이전값 저장
  currentClass = clsSym;

  // 3) (선택) 클래스 내부에서 로컬 스코프가 필요하면 enterScope/exitScope 사용
  //     다만 멤버 선언 자체는 currentClass->addMember 로 처리해야 한다.
  symbols.enterScope();
  for (auto const &m : decl->members) {
    m->accept(
        this); // VarDecl/FuncDecl 등에서 currentClass가 있으면 멤버로 등록됨
  }
  symbols.exitScope();

  insideClass=oldInClass;

  // 4) 끝나면 currentClass 복원
  currentClass = prevClass;
}

// ────────── 함수 선언 처리 ──────────
void SemanticAnalyzer::visit(FuncDecl *decl) {
  if (!insideClass)
    throw std::runtime_error("Function '" + decl->name +
                             "' must be declared inside a class.");
  SymbolInfo info;
  info.name = decl->name;
  info.type = decl->returnType->type;

  for (auto &p : decl->params)
    info.paramTypes.push_back(p->type->type);

  // 만약 현재 클래스 컨텍스트가 있으면 멤버 함수로 등록
  if (currentClass) {
    // FuncSymbol 생성 (shared_ptr)
    auto funcSym = std::make_shared<FuncSymbol>(
        decl->name, decl->returnType->type, info.paramTypes, /*level=*/0);
    if (!currentClass->addMember(funcSym)) {
      throw std::runtime_error("Duplicate member in class " +
                               std::string(currentClass->name) + ": " +
                               decl->name);
    }

    // 멤버 함수의 본문은 분석해야 하므로 내부 스코프를 설정하고 파라미터 심볼
    // 등 처리 아래는 멤버 함수 내부 처리를 위해 기존 로직의 일부를 재사용
    currentFunctionStack.push_back(
        funcSym.get()); // 주의: FuncSymbol* 사용을 위해 .get()
    symbols.enterScope();
    currentReturnType = decl->returnType->type;

    for (auto &p : decl->params) {
      SymbolInfo param;
      param.name = p->name;
      param.type = p->type->type;
      if (!symbols.define(VAR, param))
        throw std::runtime_error("Duplicate parameter: " + p->name);
    }

    for (const auto &m : decl->members)
      m->accept(this);

    symbols.exitScope();
    currentFunctionStack.pop_back();

    return;
  }

  // 클래스 외부(전역/지역) 함수는 기존 로직
  if (!symbols.define(FUNC, info))
    throw std::runtime_error("Function redefinition: " + decl->name);

  currentFunctionStack.push_back(symbols.lookupFunc(info.name));

  symbols.enterScope();
  currentReturnType = decl->returnType->type;

  for (auto &p : decl->params) {
    SymbolInfo param;
    param.name = p->name;
    param.type = p->type->type;
    if (!symbols.define(VAR, param))
      throw std::runtime_error("Duplicate parameter: " + p->name);
  }

  for (const auto &m : decl->members)
    m->accept(this);
  symbols.exitScope();
  currentFunctionStack.pop_back();
}

// ────────── 변수 선언 처리 ──────────
void SemanticAnalyzer::visit(VarDecl *decl, bool isInFor) {

  if (!insideClass)
    throw std::runtime_error("Var '" + decl->name +
                             "' must be declared inside a class.");

  // 멤버 변수로 들어가는 경우
  if (currentClass) {
    // 클래스 멤버로 추가
    // VarSymbol 객체 생성 (shared_ptr)
    auto varSym = std::make_shared<VarSymbol>(decl->name, decl->type->type,
                                              /*isConst=*/false, /*lvl=*/0);
    // 필요하면 초기값/const 정보 더 설정

    // 클래스 심볼에 멤버로 등록 (중복 검사는 addMember에서 처리)
    if (!currentClass->addMember(varSym)) {
      throw std::runtime_error("Duplicate member in class " +
                               std::string(currentClass->name) + ": " +
                               decl->name);
    }

    // 클래스 멤버는 일반 심볼테이블 스코프에 등록하지 않음 (또는 따로 static
    // 멤버라면 등록할 수 있음)
    return;
  }

  // 일반 변수는 기존 동작 유지
  if (!symbols.define(VAR,
                      SymbolInfo{.name = decl->name, .type = decl->type->type}))
    throw std::runtime_error("Duplicate variable: " + decl->name);
}

void SemanticAnalyzer::visit(ArrayDecl *decl) {
  if (!symbols.define(ARRAY, SymbolInfo{.name = decl->name,
                                        .type = decl->type->type,
                                        .index = decl->index}))
    throw std::runtime_error("Duplicate variable: " + decl->name);
}
// ────────── 변수 참조 ──────────
void SemanticAnalyzer::visit(VarExpr *expr) {
  auto *sym = symbols.lookupVar(expr->name);
  if (!sym)
    throw std::runtime_error("Undefined variable: " + expr->name);

  expr->evaluatedType = sym->type;
}

void SemanticAnalyzer::visit(LiteralExpr *expr) {}

void SemanticAnalyzer::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);

  string lhs = expr->left->evaluatedType;
  string rhs = expr->right->evaluatedType;
  string op = expr->op;

  // ─────────────────────
  // 산술 연산자
  // ─────────────────────
  if (op == "+" || op == "-" || op == "*" || op == "/" || op == "**") {
    if (lhs != "int" || rhs != "int")
      throw runtime_error("Arithmetic operator requires int");
    expr->evaluatedType = "int";
    return;
  }

  // ─────────────────────
  // 비교 연산자 (산술 기반)
  // <, >, <=, >=
  // ─────────────────────
  if (op == "<" || op == ">" || op == "<=" || op == ">=") {
    if (lhs != "int" || rhs != "int")
      throw runtime_error("Comparison operator requires int");
    expr->evaluatedType = "boolean";
    return;
  }

  // ─────────────────────
  // 동등/불일치 비교 ==, !=
  // ─────────────────────
  if (op == "==" || op == "!=") {
    if (lhs != rhs)
      throw runtime_error("Both sides of == or != must have same type");
    expr->evaluatedType = "boolean";
    return;
  }

  // ─────────────────────
  // 논리 연산자 &&, ||
  // ─────────────────────
  if (op == "&&" || op == "||") {
    if (lhs != "boolean" || rhs != "boolean")
      throw runtime_error("Logical operator requires bool");
    expr->evaluatedType = "boolean";
    return;
  }

  throw runtime_error("Unsupported operator: " + op);
}

void SemanticAnalyzer::visit(CallExpr *expr) {
  // callee는 VarExpr라고 가정
  auto *nameExpr = dynamic_cast<VarExpr *>(expr->callee.get());
  if (!nameExpr)
    throw std::runtime_error("Invalid function call target");

  auto *func = symbols.lookupFunc(nameExpr->name);
  if (!func)
    throw std::runtime_error("Undefined function: " + nameExpr->name);

  // 인자 개수 확인
  if (func->paramTypes.size() != expr->args.size())
    throw runtime_error("Argument count mismatch in call to " + nameExpr->name);

  // 인자 타입 검사
  for (size_t i = 0; i < expr->args.size(); i++) {
    expr->args[i]->accept(this);
    if (expr->args[i]->evaluatedType != func->paramTypes[i])
      throw runtime_error("Argument type mismatch in call to " +
                          nameExpr->name);
  }
}

void SemanticAnalyzer::visit(GroupExpr *expr) {
  expr->expression->accept(this);
  expr->evaluatedType = expr->expression->evaluatedType;
}

void SemanticAnalyzer::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);

  string lhs = expr->target->evaluatedType;
  string rhs = expr->value->evaluatedType;

  // 단순 대입 =
  if (expr->op == "=") {
    if (lhs != rhs)
      throw runtime_error("Type mismatch in assignment");
    expr->evaluatedType = lhs;
    return;
  }

  // 복합 대입 += -= *= /=
  if (expr->op == "+=" || expr->op == "-=" || expr->op == "*=" ||
      expr->op == "/=") {

    if (lhs != "int" || rhs != "int")
      throw runtime_error("Arithmetic assignment requires int");

    expr->evaluatedType = "int";
    return;
  }

  throw runtime_error("Unknown assignment operator: " + expr->op);
}

void SemanticAnalyzer::visit(AccessExpr *expr) {
  // 1. 왼쪽 객체 먼저 타입 계산
  expr->object->accept(this);

  string objType = expr->object->evaluatedType;
  string member = expr->memberName.text;

  if (objType == "null") {
    throw std::runtime_error("invalid member access on null object: " + member);
  }

  // 2. 타입 정보 테이블에서 objType 구조체/객체 타입 조회
  if (!symbols.hasType(objType)) {
    throw std::runtime_error("type '" + objType +
                             "' is not a struct/object type.");
  }

  ClassSymbol *typeInfo = symbols.lookupClass(objType);
  if (!typeInfo)
    throw runtime_error("type '" + objType + "' is not a struct/object type.");
  if (!typeInfo->hasMember(member))
    throw runtime_error("");

  // member 심볼 얻기
  auto memSym = typeInfo->getMember(member);
  if (!memSym)
    throw runtime_error("");

  // 멤버의 타입 결정: 만약 memSym이 VarSymbol이면 memSym->type 을 사용
  if (auto vs = dynamic_cast<VarSymbol *>(memSym.get())) {
    expr->evaluatedType = vs->type;
  } else if (auto fs = dynamic_cast<FuncSymbol *>(memSym.get())) {
    expr->evaluatedType = fs->returnType;
  } else {
    // 기타 처리
  }
}

void SemanticAnalyzer::visit(IndexExpr *expr) {
  expr->array->accept(this);
  expr->index->accept(this);

  if (expr->index->evaluatedType != "int")
    throw runtime_error("Array index must be int");

  string t = expr->array->evaluatedType;
  expr->evaluatedType = t.substr(0, t.size());
}

void SemanticAnalyzer::visit(PostfixExpr *expr) {
  expr->left->accept(this);

  if (expr->left->evaluatedType != "int")
    throw runtime_error("Postfix ++/-- requires int");

  expr->evaluatedType = "int";
}

void SemanticAnalyzer::visit(ArrayAccessExpr *expr) {
  expr->expr->accept(this);
  expr->index->accept(this);

  if (expr->index->evaluatedType != "int")
    throw runtime_error("Array index must be int");

  string t = expr->expr->evaluatedType;
  expr->evaluatedType = t.substr(0, t.size());
}

void SemanticAnalyzer::visit(UnaryExpr *expr) {
  expr->operand->accept(this);
  string t = expr->operand->evaluatedType;

  // prefix or postfix ++ --
  if (expr->op == "++" || expr->op == "--") {
    if (t != "int")
      throw runtime_error("Unary ++/-- requires int");
    expr->evaluatedType = "int";
    return;
  }

  // 단항 -
  if (expr->op == "-") {
    if (t != "int")
      throw runtime_error("Unary '-' requires int");
    expr->evaluatedType = "int";
    return;
  }

  // 논리 not
  if (expr->op == "!") {
    if (t != "boolean")
      throw runtime_error("Unary '!' requires bool");
    expr->evaluatedType = "boolean";
    return;
  }

  throw runtime_error("Unknown unary operator: " + expr->op);
}
void SemanticAnalyzer::visit(ExprStmt *stmt) {}
void SemanticAnalyzer::visit(BlockStmt *stmt) {
  symbols.enterScope();
  for (auto const &s : stmt->statements) {
    s->accept(this);
  }
  symbols.exitScope();
}
void SemanticAnalyzer::visit(IfStmt *stmt) {
  stmt->condition->accept(this);
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void SemanticAnalyzer::visit(ForStmt *stmt) {
  symbols.enterScope(); // for 스코프 시작

  canBreak++;
  canContinue++;

  // ---- initializer 처리 ----
  if (auto p = std::get_if<std::shared_ptr<Stmt>>(&stmt->initializer)) {
    if (*p)
      (*p)->accept(this);
  } else if (auto p = std::get_if<std::shared_ptr<Expr>>(&stmt->initializer)) {
    if (*p)
      (*p)->accept(this);
  } else {
    // nullptr_t 인 경우: 아무 것도 안 함
  }

  // ---- condition 처리 ----
  if (stmt->condition)
    stmt->condition->accept(this);

  // ---- increment 처리 ----
  if (stmt->increment)
    stmt->increment->accept(this);

  // ---- body 처리 ----
  stmt->body->accept(this);

  canBreak--;
  canContinue--;

  symbols.exitScope(); // for 스코프 종료
}
void SemanticAnalyzer::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->left->accept(this);
  expr->right->accept(this);

  if (expr->conditon->evaluatedType != "boolean")
    throw runtime_error("Ternary condition must be boolean");

  if (expr->left->evaluatedType != expr->right->evaluatedType)
    throw runtime_error("Ternary branches must match in type");

  expr->evaluatedType = expr->left->evaluatedType;
}

void SemanticAnalyzer::visit(WhileStmt *stmt) {
  SemanticAnalyzer::canBreak++;
  SemanticAnalyzer::canContinue++;
  stmt->condition->accept(this);
  stmt->body->accept(this);
  SemanticAnalyzer::canBreak--;
  SemanticAnalyzer::canContinue--;
}
void SemanticAnalyzer::visit(SwitchStmt *stmt) {
  SemanticAnalyzer::canBreak++;
  stmt->expression->accept(this);
  for (auto const &c : stmt->cases)
    c->accept(this);
  SemanticAnalyzer::canBreak--;
}
void SemanticAnalyzer::visit(CaseStmt *stmt) {
  for (auto const &b : stmt->body)
    b->accept(this);
}
void SemanticAnalyzer::visit(ReturnStmt *stmt) {
  if (currentFunctionStack.front()->returnType == "void") {
    if (stmt->value != nullptr) {
      throw runtime_error("void has no return value");
    }
  }

  stmt->value->accept(this);

  if (currentFunctionStack.front()->returnType != stmt->value->evaluatedType) {
    string message =
        "no matched return value:" + currentFunctionStack.front()->returnType +
        ", " + stmt->value->evaluatedType;
    throw runtime_error(message);
  }
}
void SemanticAnalyzer::visit(BreakStmt *stmt) {
  if (SemanticAnalyzer::canBreak == 0)
    throw runtime_error("'break' is not allowed here");
}
void SemanticAnalyzer::visit(ContinueStmt *stmt) {
  if (SemanticAnalyzer::canContinue == 0)
    throw runtime_error("'continue' is not allowed here");
}
void SemanticAnalyzer::visit(EmptyStmt *stmt) {}

void SemanticAnalyzer::visit(StructDecl *decl) {}
void SemanticAnalyzer::visit(EnumDecl *decl) {}
void SemanticAnalyzer::visit(InterfaceDecl *decl) {}

void SemanticAnalyzer::visit(TypeNode *decl) {}
void SemanticAnalyzer::visit(ASTNode *node) {}
