#include "hrd/InitChecker/InitChecker.h"

#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/enums/HirExpect.h"
#include "hrd/util/Error.h"

#include "magic_enum/magic_enum.hpp"

#include <string>
#include <type_traits>
#include <utility>

InitChecker::InitChecker(InitChecerContext &ctx)
    : program(ctx.program), initFields(ctx.summary.initFields),
      commonFields(ctx.summary.commonFields), engine(ctx.engine) {}

void InitChecker::check() {
  if (program == nullptr) {
    Error::internal("InitChecker: program is nullptr");
  }

  prepareImportedSummary();

  for (auto &[_, type] : program->typeDeclMap) {
    if (type == nullptr) {
      Error::internal("InitChecker: type is nullptr");
    }

    checkType(type);
  }
}

void InitChecker::prepareImportedSummary() {
  for (auto &[method, _] : initFields) {
    if (method == nullptr || method->owner == nullptr) {
      continue;
    }

    TypeSymbol *type = method->owner;

    if (commonFields.find(type) != commonFields.end()) {
      continue;
    }

    bool first = true;
    FieldSet common;
    if (auto obi = dyn_cast<ObjectType>(type)) {
      for (auto *init : obi->inits) {
        auto it = initFields.find(init);

        if (it == initFields.end()) {
          continue;
        }

        if (first) {
          common = it->second;
          first = false;
        } else {
          common = mergeField(common, it->second);
        }
      }
    }

    commonFields[type] = std::move(common);
  }
}

void InitChecker::checkType(HIRTypeDecl *type) {
  currentType = type;

  bool firstInit = true;
  FieldSet common;

  /*
   * init을 먼저 전부 검사해야 한다.
   *
   * 각 init이 최종적으로 보장하는 field set을 얻은 뒤
   * 교집합을 계산해야 일반 method에서 self.field 사용 여부를
   * 판단할 수 있기 때문이다.
   */
  for (auto &[_, init] : type->initMap) {
    if (init == nullptr) {
      Error::internal(type->span, "InitChecker: init is nullptr");
    }

    checkInit(init);

    auto it = initFields.find(init->symbol);
    if (it == initFields.end()) {
      Error::internal(init->span,
                      "InitChecker: failed to create init field state");
    }

    if (firstInit) {
      common = it->second;
      firstInit = false;
    } else {
      common = mergeField(common, it->second);
    }
  }

  /*
   * init이 하나도 없으면 보장 가능한 field 역시 없다.
   *
   * 만약 언어에서 암묵적 default init을 항상 생성한다면
   * 실제로는 이 경우가 거의 없을 수도 있다.
   */
  if (firstInit) {
    common.clear();
  }

  commonFields[type->type] = std::move(common);

  /*
   * 이제 모든 init 정보가 준비됐으므로 일반 method 검사.
   */
  for (auto &[_, method] : type->methodMap) {
    if (method == nullptr) {
      Error::internal(type->span, "InitChecker: method is nullptr");
    }

    checkMethod(method);
  }

  currentType = nullptr;
}

void InitChecker::checkInit(HIRMethodDecl *method) {
  if (method == nullptr) {
    Error::internal("InitChecker: init is nullptr");
  }

  if (method->body == nullptr) {
    Error::internal(method->span, "InitChecker: init body is nullptr");
  }

  checkingInit = true;
  currentSelfFields.clear();

  InitState state;

  addMethodEntryState(method, state);

  /*
   * 선언부 field initializer.
   *
   * 예:
   *
   * struct Foo {
   *   i32 x = 10;
   *   i32 y;
   * }
   *
   * defaultInitBlock이 self.x = 10 형태로 이미 lowering되어
   * 있다고 가정한다.
   */
  if (currentType != nullptr && currentType->defaultInitBlock != nullptr) {
    checkBlock(currentType->defaultInitBlock.get(), state);
  }

  checkBlock(method->body.get(), state);

  /*
   * 이 init이 최종적으로 보장하는 field set.
   *
   * "모든 field가 초기화되어야 한다"는 검사는 하지 않는다.
   * 초기화된 field만 이 init이 제공하는 유효 field가 된다.
   */
  initFields[method->symbol] = currentSelfFields;

  currentSelfFields.clear();
  checkingInit = false;
}

void InitChecker::checkMethod(HIRMethodDecl *method) {
  if (method == nullptr) {
    Error::internal("InitChecker: method is nullptr");
  }

  if (method->isExtern) {
    return;
  }

  if (method->body == nullptr) {
    Error::internal(method->span, "InitChecker: method body is nullptr");
  }

  checkingInit = false;

  InitState state;

  addMethodEntryState(method, state);

  checkBlock(method->body.get(), state);
}

void InitChecker::addMethodEntryState(HIRMethodDecl *method, InitState &state) {
  for (auto &[_, param] : method->paramMap) {
    if (param == nullptr) {
      Error::internal(method->span, "InitChecker: param is nullptr");
    }

    /*
     * HIRParam의 실제 ValueSymbol 멤버 이름에 맞춰 수정.
     */
    ValueSymbol *symbol = param->symbol;

    if (symbol == nullptr) {
      Error::internal(method->span, "InitChecker: param symbol is nullptr");
    }

    state.values.insert(symbol);

    /*
     * struct parameter는 호출자가 어떤 init으로 생성했는지
     * 이 method만으로 알 수 없다.
     *
     * 따라서 해당 type의 모든 init이 공통으로 보장하는
     * field만 사용할 수 있다.
     */
    if (param->type != nullptr && param->type->kind == TypeKind::STRUCT) {

      auto it = commonFields.find(param->type);

      if (it != commonFields.end()) {
        state.fields[symbol] = it->second;
      } else {
        state.fields[symbol] = {};
      }
    }
  }
}

void InitChecker::checkBlock(HIRBlockStmt *block, InitState &state) {
  if (block == nullptr) {
    Error::internal("InitChecker: block is nullptr");
  }

  for (auto &stmt : block->statements) {
    if (stmt == nullptr) {
      Error::internal(block->span, "InitChecker: stmt is nullptr");
    }

    checkStmt(stmt.get(), state);
  }

  /*
   * 현재 block에서 선언된 local은 scope 밖으로 나가므로 제거.
   *
   * 바깥 scope에서 선언된 local의 상태 변경은 그대로 살아있다.
   */
  for (auto &[_, local] : block->localMap) {
    if (local == nullptr) {
      continue;
    }

    ValueSymbol *symbol = local->symbol;

    if (symbol == nullptr) {
      continue;
    }

    state.values.erase(symbol);
    state.fields.erase(symbol);
  }
}

void InitChecker::checkStmt(HIRStmt *stmt, InitState &state) {
  if (stmt == nullptr) {
    Error::internal("InitChecker: stmt is nullptr");
  }

  switch (stmt->kind) {

  case HIRNodeKind::BlockStmt: {
    auto *block = expect<HIRBlockStmt>(stmt, HIRNodeKind::BlockStmt);

    checkBlock(block, state);
    break;
  }

  case HIRNodeKind::ExprStmt: {
    auto *exprStmt = expect<HIRExprStmt>(stmt, HIRNodeKind::ExprStmt);

    if (exprStmt->expr != nullptr) {
      checkExpr(exprStmt->expr.get(), state);
    }

    break;
  }

  case HIRNodeKind::LocalDeclStmt: {
    auto *local = expect<HIRLocalDeclStmt>(stmt, HIRNodeKind::LocalDeclStmt);

    if (local->local == nullptr) {
      Error::internal(stmt->span,
                      "InitChecker: local declaration has no local");
    }

    /*
     * initializer가 없는 struct는 storage 자체는 존재하지만
     * 아직 초기화된 field가 하나도 없는 상태로 시작한다.
     *
     *   Foo a;
     *   a.x = 10;
     *
     * 처럼 field별 부분 초기화를 허용하기 위한 상태다.
     * scalar/array는 기존처럼 미초기화 상태로 남긴다.
     */
    if (local->init == nullptr) {
      if (local->local->type != nullptr &&
          local->local->type->kind == TypeKind::STRUCT) {
        ValueSymbol *symbol = local->local->symbol;

        if (symbol == nullptr) {
          Error::internal(stmt->span, "InitChecker: local symbol is nullptr");
        }

        state.values.insert(symbol);
        state.fields[symbol] = {};
      }

      break;
    }

    /*
     * initializer에서 미초기화 값을 읽는지 먼저 검사.
     */
    checkExpr(local->init.get(), state);

    ValueSymbol *symbol = local->local->symbol;

    if (symbol == nullptr) {
      Error::internal(stmt->span, "InitChecker: local symbol is nullptr");
    }

    state.values.insert(symbol);

    /*
     * struct value라면 어떤 field들이 보장되는 값인지 같이 전달.
     */
    if (local->local->type != nullptr &&
        local->local->type->kind == TypeKind::STRUCT) {

      state.fields[symbol] = getExprFields(local->init.get(), state);
    }

    break;
  }

  case HIRNodeKind::AssignStmt: {
    auto *assign = expect<HIRAssignStmt>(stmt, HIRNodeKind::AssignStmt);

    if (assign->lhs == nullptr) {
      Error::internal(stmt->span, "InitChecker: assignment lhs is nullptr");
    }

    if (assign->rhs == nullptr) {
      Error::internal(stmt->span, "InitChecker: assignment rhs is nullptr");
    }

    /*
     * 반드시 RHS를 먼저 검사.
     *
     * i32 x;
     * x = x;
     *
     * 같은 코드는 RHS에서 오류가 나야 한다.
     */
    checkExpr(assign->rhs.get(), state);

    FieldSet rhsFields;

    if (assign->lhs->type != nullptr &&
        assign->lhs->type->kind == TypeKind::STRUCT) {

      rhsFields = getExprFields(assign->rhs.get(), state);
    }

    checkWrite(assign->lhs.get(), state);

    /*
     * direct struct value assignment라면
     * RHS의 field guarantee를 LHS로 복사한다.
     */
    ValueSymbol *target = getValueSymbol(assign->lhs.get());

    if (target != nullptr && assign->lhs->type != nullptr &&
        assign->lhs->type->kind == TypeKind::STRUCT) {

      state.fields[target] = std::move(rhsFields);
    }

    break;
  }

  case HIRNodeKind::CompoundAssignStmt: {
    auto *compound =
        expect<HIRCompoundAssignStmt>(stmt, HIRNodeKind::CompoundAssignStmt);

    if (compound->lhs == nullptr || compound->rhs == nullptr) {
      Error::internal(stmt->span, "InitChecker: invalid compound assignment");
    }

    /*
     * compound assignment는 기존 값을 읽는다.
     *
     * x += 1
     *
     * 은 initialization이 될 수 없다.
     */
    checkRead(compound->lhs.get(), state);
    checkExpr(compound->rhs.get(), state);

    break;
  }

  case HIRNodeKind::IfStmt: {
    auto *ifs = expect<HIRIfStmt>(stmt, HIRNodeKind::IfStmt);

    if (ifs->condition == nullptr || ifs->thenBlock == nullptr) {
      Error::internal(stmt->span, "InitChecker: invalid if statement");
    }

    checkExpr(ifs->condition.get(), state);

    InitState thenState = state;

    checkBlock(ifs->thenBlock.get(), thenState);

    if (ifs->elseBlock != nullptr) {
      InitState elseState = state;

      checkBlock(ifs->elseBlock.get(), elseState);

      state = mergeInit(thenState, elseState);
    } else {
      /*
       * then이 실행되지 않는 경로가 존재한다.
       */
      state = mergeInit(state, thenState);
    }

    break;
  }

  case HIRNodeKind::WhileStmt: {
    auto *loop = expect<HIRWhileStmt>(stmt, HIRNodeKind::WhileStmt);

    if (loop->condition == nullptr || loop->body == nullptr) {
      Error::internal(stmt->span, "InitChecker: invalid while statement");
    }

    checkExpr(loop->condition.get(), state);

    InitState bodyState = state;

    checkBlock(loop->body.get(), bodyState);

    /*
     * while이 0번 실행될 수 있으므로
     * body에서 발생한 initialization은 밖으로 내보내지 않는다.
     */

    break;
  }

  case HIRNodeKind::ForRangeStmt: {
    auto *range = expect<HIRForRangeStmt>(stmt, HIRNodeKind::ForRangeStmt);

    if (range->start == nullptr || range->end == nullptr ||
        range->step == nullptr || range->indexVar == nullptr ||
        range->body == nullptr) {
      Error::internal(stmt->span, "InitChecker: invalid range statement");
    }

    checkExpr(range->start.get(), state);
    checkExpr(range->end.get(), state);
    checkExpr(range->step.get(), state);

    InitState bodyState = state;

    ValueSymbol *indexSymbol = range->indexVar->symbol;

    if (indexSymbol == nullptr) {
      Error::internal(stmt->span, "InitChecker: range index symbol is nullptr");
    }

    /*
     * loop body 안에서는 index가 항상 초기화되어 있다.
     */
    bodyState.values.insert(indexSymbol);

    checkBlock(range->body.get(), bodyState);

    /*
     * range 역시 0번 실행될 수 있으므로
     * body state는 밖으로 전달하지 않는다.
     */

    break;
  }

  case HIRNodeKind::ReturnStmt: {
    auto *ret = expect<HIRReturnStmt>(stmt, HIRNodeKind::ReturnStmt);

    if (ret->value != nullptr) {
      checkExpr(ret->value.get(), state);
    }

    break;
  }

  case HIRNodeKind::BreakStmt:
  case HIRNodeKind::ContinueStmt:
  case HIRNodeKind::QuitStmt:
    break;

  case HIRNodeKind::SwitchStmt: {
    auto *sw = expect<HIRSwitchStmt>(stmt, HIRNodeKind::SwitchStmt);

    if (sw->cond == nullptr) {
      Error::internal(stmt->span, "InitChecker: switch condition is nullptr");
    }

    checkExpr(sw->cond.get(), state);

    bool firstCase = true;
    bool hasDefault = false;

    InitState merged;

    for (auto &casePtr : sw->cases) {
      if (casePtr == nullptr) {
        Error::internal(stmt->span, "InitChecker: switch case is nullptr");
      }

      InitState caseState = state;

      checkCase(casePtr.get(), caseState);

      if (casePtr->defaultKind == HIRDefaultKind::Default) {
        hasDefault = true;
      }

      if (firstCase) {
        merged = std::move(caseState);
        firstCase = false;
      } else {
        merged = mergeInit(merged, caseState);
      }
    }

    if (firstCase) {
      break;
    }

    /*
     * default가 없으면 아무 case에도 진입하지 않는 경로 존재.
     */
    if (!hasDefault) {
      merged = mergeInit(merged, state);
    }

    state = std::move(merged);

    break;
  }

  case HIRNodeKind::Case: {
    auto *caseStmt = expect<HIRCase>(stmt, HIRNodeKind::Case);

    checkCase(caseStmt, state);
    break;
  }

  case HIRNodeKind::ValueTransferStmt: {
    auto *transfer =
        expect<HIRValueTransferStmt>(stmt, HIRNodeKind::ValueTransferStmt);

    if (transfer->value != nullptr) {
      checkExpr(transfer->value.get(), state);
    }

    break;
  }

  case HIRNodeKind::DestroyStmt: {
    auto *destroy = expect<HIRDestroyStmt>(stmt, HIRNodeKind::DestroyStmt);

    if (destroy->handle != nullptr) {
      checkExpr(destroy->handle.get(), state);
    }

    break;
  }

  case HIRNodeKind::OnExitStmt:
    /*
     * verifier에서도 현재 별도 처리 상태.
     * 현재 initialization에는 영향 없음.
     */
    break;

  default:
    Error::internal(stmt->span,
                    "InitChecker: illegal stmt kind: " +
                        std::string(magic_enum::enum_name(stmt->kind)));
  }
}

void InitChecker::checkExpr(HIRExpr *expr, InitState &state) {
  if (expr == nullptr) {
    Error::internal("InitChecker: expr is nullptr");
  }

  switch (expr->kind) {

  case HIRNodeKind::LiteralExpr:
  case HIRNodeKind::SelfExpr:
  case HIRNodeKind::RootExpr:
    break;

  case HIRNodeKind::ArrayLiteralExpr: {
    auto *literal =
        expect<HIRArrayLiteralExpr>(expr, HIRNodeKind::ArrayLiteralExpr);

    for (auto &element : literal->elements) {
      if (element != nullptr) {
        checkExpr(element.get(), state);
      }
    }

    break;
  }

  case HIRNodeKind::LoadExpr: {
    auto *load = expect<HIRLoadExpr>(expr, HIRNodeKind::LoadExpr);

    if (load->place == nullptr) {
      Error::internal(expr->span, "InitChecker: load place is nullptr");
    }

    /*
     * 실제 value read의 경계.
     */
    checkRead(load->place.get(), state);

    break;
  }

  case HIRNodeKind::UnaryExpr: {
    auto *unary = expect<HIRUnaryExpr>(expr, HIRNodeKind::UnaryExpr);

    checkExpr(unary->operand.get(), state);

    break;
  }

  case HIRNodeKind::BinaryExpr: {
    auto *binary = expect<HIRBinaryExpr>(expr, HIRNodeKind::BinaryExpr);

    checkExpr(binary->left.get(), state);
    checkExpr(binary->right.get(), state);

    break;
  }

  case HIRNodeKind::CastExpr: {
    auto *cast = expect<HIRCastExpr>(expr, HIRNodeKind::CastExpr);

    checkExpr(cast->operand.get(), state);

    break;
  }

  case HIRNodeKind::MethodCallExpr: {
    auto *call = expect<HIRMethodCallExpr>(expr, HIRNodeKind::MethodCallExpr);

    if (!call->method->isStatic) {
      if (call->receiver == nullptr) {
        Error::internal(expr->span, "InitChecker: method receiver is nullptr");
      }
      checkRead(call->receiver.get(), state);
    }

    for (auto &arg : call->args) {
      if (arg != nullptr) {
        checkExpr(arg.get(), state);
      }
    }

    break;
  }

  case HIRNodeKind::SpawnExpr: {
    auto *spawn = expect<HIRSpawnExpr>(expr, HIRNodeKind::SpawnExpr);

    for (auto &arg : spawn->args) {
      if (arg != nullptr) {
        checkExpr(arg.get(), state);
      }
    }

    break;
  }

  case HIRNodeKind::ViewExpr: {
    auto *view = expect<HIRViewExpr>(expr, HIRNodeKind::ViewExpr);

    if (view->handle != nullptr) {
      checkExpr(view->handle.get(), state);
    }

    break;
  }

  case HIRNodeKind::MatchExpr: {
    auto *match = expect<HIRMatchExpr>(expr, HIRNodeKind::MatchExpr);

    if (match->cond == nullptr) {
      Error::internal(expr->span, "InitChecker: match condition is nullptr");
    }

    checkExpr(match->cond.get(), state);

    bool firstCase = true;
    bool hasDefault = false;

    InitState merged;

    for (auto &casePtr : match->cases) {
      if (casePtr == nullptr) {
        Error::internal(expr->span, "InitChecker: match case is nullptr");
      }

      InitState caseState = state;

      checkCase(casePtr.get(), caseState);

      if (casePtr->defaultKind == HIRDefaultKind::Default) {
        hasDefault = true;
      }

      if (firstCase) {
        merged = std::move(caseState);
        firstCase = false;
      } else {
        merged = mergeInit(merged, caseState);
      }
    }

    if (!firstCase) {
      if (!hasDefault) {
        merged = mergeInit(merged, state);
      }

      state = std::move(merged);
    }

    break;
  }

  case HIRNodeKind::TernaryExpr: {
    auto *ternary = expect<HIRTernaryExpr>(expr, HIRNodeKind::TernaryExpr);

    checkExpr(ternary->condition.get(), state);

    InitState thenState = state;
    InitState elseState = state;

    checkExpr(ternary->thenExpr.get(), thenState);
    checkExpr(ternary->elseExpr.get(), elseState);

    state = mergeInit(thenState, elseState);

    break;
  }

  case HIRNodeKind::EnumVariantValue: {
    auto *variant =
        expect<HIRVariantValueExpr>(expr, HIRNodeKind::EnumVariantValue);

    if (variant->payload != nullptr) {
      checkExpr(variant->payload.get(), state);
    }

    break;
  }

  case HIRNodeKind::StructInitExpr: {
    auto *init = expect<HIRStructInitExpr>(expr, HIRNodeKind::StructInitExpr);

    /*
     * struct init expression 자체는 완성된 struct value를 만든다.
     * 여기서는 arguments의 read만 검증.
     */
    for (auto &arg : init->args) {
      if (arg != nullptr) {
        checkExpr(arg.get(), state);
      }
    }

    break;
  }

  case HIRNodeKind::RuntimeCallExpr: {
    auto *runtime = expect<HIRRuntimeCall>(expr, HIRNodeKind::RuntimeCallExpr);

    for (auto &arg : runtime->args) {
      if (arg != nullptr) {
        checkExpr(arg.get(), state);
      }
    }

    break;
  }

  /*
   * place 자체는 read가 아니다.
   *
   * LoadExpr 또는 checkRead/checkWrite가 의미를 결정한다.
   */
  case HIRNodeKind::LocalPlaceExpr:
  case HIRNodeKind::ParamPlaceExpr:
  case HIRNodeKind::FieldPlaceExpr:
    break;

  case HIRNodeKind::ArrayAccessExpr: {
    auto *array =
        expect<HIRArrayAccessPlaceExpr>(expr, HIRNodeKind::ArrayAccessExpr);

    /*
     * place 주소를 계산하려면 index expression은 평가된다.
     *
     * object 자체를 read하는지는
     * checkRead / checkWrite가 결정한다.
     */
    if (array->index != nullptr) {
      checkExpr(array->index.get(), state);
    }

    break;
  }

  case HIRNodeKind::DefaultValueExpr:
    Error::internal(
        expr->span,
        "InitChecker: DefaultValueExpr remained after HIR lowering");
    break;

  default:
    Error::internal(expr->span,
                    "InitChecker: illegal expr kind: " +
                        std::string(magic_enum::enum_name(expr->kind)));
  }
}

void InitChecker::checkRead(HIRExpr *expr, InitState &state) {
  if (expr == nullptr) {
    Error::internal("InitChecker: read expr is nullptr");
  }

  switch (expr->kind) {

  case HIRNodeKind::LocalPlaceExpr: {
    auto *local = expect<HIRLocalPlaceExpr>(expr, HIRNodeKind::LocalPlaceExpr);

    if (local->local == nullptr) {
      Error::internal(expr->span, "InitChecker: local place has no local");
    }

    requireInitialized(local->local->symbol, expr, state);

    break;
  }

  case HIRNodeKind::ParamPlaceExpr: {
    auto *param = expect<HIRParamPlaceExpr>(expr, HIRNodeKind::ParamPlaceExpr);

    if (param->param == nullptr) {
      Error::internal(expr->span, "InitChecker: param place has no param");
    }

    requireInitialized(param->param->symbol, expr, state);

    break;
  }

  case HIRNodeKind::FieldPlaceExpr: {
    auto *field = expect<HIRFieldPlaceExpr>(expr, HIRNodeKind::FieldPlaceExpr);

    if (field->receiver == nullptr || field->field == nullptr) {
      Error::internal(expr->span, "InitChecker: invalid field place");
    }

    /*
     * self.field
     */
    if (field->receiver->kind == HIRNodeKind::SelfExpr) {

      if (checkingInit) {
        /*
         * 현재 init에서 지금까지 실제로 초기화된 field만
         * 읽을 수 있다.
         */
        requireSelfField(field->field, expr);
      } else {
        /*
         * 일반 method에서는 모든 init이 공통으로
         * 초기화하는 field만 사용할 수 있다.
         */
        if (currentType == nullptr || currentType->type == nullptr) {
          Error::internal(expr->span, "InitChecker: current type is nullptr");
        }

        auto it = commonFields.find(currentType->type);

        if (it == commonFields.end() || !contain(it->second, field->field)) {

          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H006);

          dia.labels = {
              {expr->span,
               "field '" + field->field->name +
                   "' is not initialized by every initializer",
               true},
          };

          dia.notes = {
              "methods may only use fields initialized by every initializer",
          };

          dia.helps = {
              "initialize '" + field->field->name + "' in every initializer",
          };

          engine.emit(dia);
        }
      }

      break;
    }

    /*
     * 일반 struct expression의 field.
     *
     * 먼저 aggregate 자체가 initialized인지 확인.
     */
    checkRead(field->receiver.get(), state);

    ValueSymbol *owner = getValueSymbol(field->receiver.get());

    if (owner != nullptr) {
      /*
       * local/param처럼 구체적인 value 상태를 알고 있다.
       */
      requireField(owner, field->field, expr, state);
    } else {
      /*
       * method call 결과 등 어떤 init에서 왔는지
       * 추적할 수 없는 struct expression.
       *
       * type 공통 field만 안전하게 보장된다.
       */
      TypeSymbol *type = field->receiver->type;

      auto it = commonFields.find(type);

      if (it == commonFields.end() || !contain(it->second, field->field)) {

        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H006);

        dia.labels = {
            {expr->span,
             "field '" + field->field->name +
                 "' is not guaranteed to be initialized",
             true},
        };

        engine.emit(dia);
      }
    }

    break;
  }

  case HIRNodeKind::ArrayAccessExpr: {
    auto *array =
        expect<HIRArrayAccessPlaceExpr>(expr, HIRNodeKind::ArrayAccessExpr);

    if (array->object == nullptr || array->index == nullptr) {
      Error::internal(expr->span, "InitChecker: invalid array access");
    }

    /*
     * 배열 element는 별도 initialization state를 갖지 않는다.
     *
     * array 전체가 initialized되어 있어야 element 접근 가능.
     */
    checkRead(array->object.get(), state);
    checkExpr(array->index.get(), state);

    break;
  }

  case HIRNodeKind::SelfExpr: {
    /*
     * 일반 method에서 self는 완성된 객체.
     *
     * init 도중 self 자체를 value로 넘기거나 읽는 것을
     * 금지할 거라면 여기서 전용 diagnostic을 추가하면 된다.
     */
    break;
  }

  case HIRNodeKind::RootExpr:
    break;

  default:
    /*
     * place가 아닌 expression이면 일반 expression 검사.
     */
    checkExpr(expr, state);
    break;
  }
}

void InitChecker::checkWrite(HIRExpr *expr, InitState &state) {
  if (expr == nullptr) {
    Error::internal("InitChecker: write expr is nullptr");
  }

  switch (expr->kind) {

  case HIRNodeKind::LocalPlaceExpr: {
    auto *local = expect<HIRLocalPlaceExpr>(expr, HIRNodeKind::LocalPlaceExpr);

    if (local->local == nullptr || local->local->symbol == nullptr) {
      Error::internal(expr->span, "InitChecker: invalid local write");
    }

    /*
     * direct local assignment는 initialization이 될 수 있다.
     */
    state.values.insert(local->local->symbol);

    break;
  }

  case HIRNodeKind::ParamPlaceExpr: {
    auto *param = expect<HIRParamPlaceExpr>(expr, HIRNodeKind::ParamPlaceExpr);

    if (param->param == nullptr || param->param->symbol == nullptr) {
      Error::internal(expr->span, "InitChecker: invalid param write");
    }

    /*
     * parameter는 진입 시 이미 initialized 상태.
     */
    state.values.insert(param->param->symbol);

    break;
  }

  case HIRNodeKind::FieldPlaceExpr: {
    auto *field = expect<HIRFieldPlaceExpr>(expr, HIRNodeKind::FieldPlaceExpr);

    if (field->receiver == nullptr || field->field == nullptr) {
      Error::internal(expr->span, "InitChecker: invalid field write");
    }

    /*
     * init에서 self.field = ...
     *
     * 이 경우만 미완성 struct의 field initialization으로 취급.
     */
    if (checkingInit && field->receiver->kind == HIRNodeKind::SelfExpr) {

      currentSelfFields.insert(field->field);

      break;
    }

    /*
     * 일반 struct value의 field write는 해당 field의 initialization이
     * 될 수 있다.
     *
     *   Foo a;
     *   a.x = 10;
     *
     * 따라서 receiver가 추적 가능한 struct value라면 기존 field를
     * 읽지 않고 해당 field를 initialized set에 추가한다.
     */
    ValueSymbol *owner = getValueSymbol(field->receiver.get());

    if (owner != nullptr) {
      /*
       * receiver storage 자체는 존재해야 한다.
       * 미초기화 struct local도 선언 시 values에 들어간다.
       */
      requireInitialized(owner, field->receiver.get(), state);
      state.fields[owner].insert(field->field);
      break;
    }

    /*
     * 추적할 수 없는 임시 struct expression 등에 대한 field write는
     * receiver가 정상적으로 읽을 수 있는 값이어야 한다.
     */
    checkRead(field->receiver.get(), state);

    break;
  }

  case HIRNodeKind::ArrayAccessExpr: {
    auto *array =
        expect<HIRArrayAccessPlaceExpr>(expr, HIRNodeKind::ArrayAccessExpr);

    if (array->object == nullptr || array->index == nullptr) {
      Error::internal(expr->span, "InitChecker: invalid array write");
    }

    /*
     * array element write 역시 initialization이 아니라 mutation.
     *
     * i32[4] a;
     * a[0] = 1;
     *
     * 은 a 자체가 미초기화이므로 오류.
     */
    checkRead(array->object.get(), state);
    checkExpr(array->index.get(), state);

    break;
  }

  default:
    Error::internal(expr->span,
                    "InitChecker: illegal assignment target: " +
                        std::string(magic_enum::enum_name(expr->kind)));
  }
}

void InitChecker::checkCase(HIRCase *caseStmt, InitState &state) {
  if (caseStmt == nullptr) {
    Error::internal("InitChecker: case is nullptr");
  }

  for (auto &selector : caseStmt->selectors) {
    if (selector == nullptr) {
      Error::internal(caseStmt->span, "InitChecker: case selector is nullptr");
    }

    checkCasePattern(selector.get(), state);
  }

  if (caseStmt->body == nullptr) {
    Error::internal(caseStmt->span, "InitChecker: case body is nullptr");
  }

  checkBlock(caseStmt->body.get(), state);
}

void InitChecker::checkCasePattern(HIRCasePattern *pattern, InitState &state) {
  if (pattern == nullptr) {
    Error::internal("InitChecker: case pattern is nullptr");
  }

  std::visit(
      [&](auto &selector) {
        using T = std::decay_t<decltype(selector)>;

        if constexpr (std::is_same_v<T, HIRLiteralCase>) {

          if (selector.expr != nullptr) {
            checkExpr(selector.expr.get(), state);
          }

        } else if constexpr (std::is_same_v<T, HIRUnitCase>) {

          /*
           * 별도 initialization 변화 없음.
           */

        } else if constexpr (std::is_same_v<T, HIRPayloadCase>) {

          /*
           * payload binding은 해당 case로 진입한 순간
           * 이미 유효한 값.
           */
          if (selector.binding != nullptr &&
              selector.binding->symbol != nullptr) {

            state.values.insert(selector.binding->symbol);

            /*
             * payload 자체가 struct라면
             * 구체적인 init provenance를 모르므로
             * type common fields만 보장.
             */
            if (selector.binding->type != nullptr &&
                selector.binding->type->kind == TypeKind::STRUCT) {

              auto it = commonFields.find(selector.binding->type);

              if (it != commonFields.end()) {
                state.fields[selector.binding->symbol] = it->second;
              }
            }
          }

        } else if constexpr (std::is_same_v<T, HIRWildcardCase>) {

          /*
           * 아무것도 없음.
           */
        }
      },
      pattern->selector);
}

InitState InitChecker::mergeInit(const InitState &lhs, const InitState &rhs) {

  InitState result;

  /*
   * value 자체가 양쪽 경로 모두에서 initialized여야 한다.
   */
  for (auto *value : lhs.values) {
    if (!contain(rhs.values, value)) {
      continue;
    }

    result.values.insert(value);

    /*
     * struct field guarantee도 양쪽 경로의 교집합만 유지.
     */
    auto lhsField = lhs.fields.find(value);

    auto rhsField = rhs.fields.find(value);

    if (lhsField != lhs.fields.end() && rhsField != rhs.fields.end()) {

      result.fields[value] = mergeField(lhsField->second, rhsField->second);
    }
  }

  return result;
}

FieldSet InitChecker::mergeField(const FieldSet &lhs, const FieldSet &rhs) {

  FieldSet result;

  const FieldSet *small = &lhs;
  const FieldSet *large = &rhs;

  if (lhs.size() > rhs.size()) {
    small = &rhs;
    large = &lhs;
  }

  for (auto *field : *small) {
    if (large->find(field) != large->end()) {
      result.insert(field);
    }
  }

  return result;
}

FieldSet InitChecker::getExprFields(HIRExpr *expr, const InitState &state) {

  if (expr == nullptr || expr->type == nullptr ||
      expr->type->kind != TypeKind::STRUCT) {
    return {};
  }

  /*
   * local / param / load(local) 등 이미 추적 중인 value라면
   * 그 value의 실제 field set을 그대로 가져간다.
   */
  if (ValueSymbol *symbol = getValueSymbol(expr); symbol != nullptr) {

    auto it = state.fields.find(symbol);

    if (it != state.fields.end()) {
      return it->second;
    }
  }

  /*
   * 특정 struct init expression.
   *
   * HIRStructInitExpr에 선택된 init HIRMethodDecl*이
   * 저장되어 있다고 가정한다.
   */
  if (expr->kind == HIRNodeKind::StructInitExpr) {
    auto *structInit =
        expect<HIRStructInitExpr>(expr, HIRNodeKind::StructInitExpr);

    /*
     * 실제 멤버명이 다르면 여기만 수정.
     */
    auto *selectedInit = structInit->method;

    if (selectedInit != nullptr) {
      auto it = initFields.find(selectedInit);

      if (it != initFields.end()) {
        return it->second;
      }
    }
  }

  /*
   * 어떤 init으로 만들어졌는지 알 수 없는 struct expression.
   *
   * 예:
   *   foo.getStruct()
   *
   * 반환 type의 common field만 안전하게 사용할 수 있다.
   */
  auto it = commonFields.find(expr->type);

  if (it != commonFields.end()) {
    return it->second;
  }

  return {};
}

ValueSymbol *InitChecker::getValueSymbol(HIRExpr *expr) {

  if (expr == nullptr) {
    return nullptr;
  }

  switch (expr->kind) {

  case HIRNodeKind::LocalPlaceExpr: {
    auto *local = expect<HIRLocalPlaceExpr>(expr, HIRNodeKind::LocalPlaceExpr);

    if (local->local == nullptr) {
      return nullptr;
    }

    return local->local->symbol;
  }

  case HIRNodeKind::ParamPlaceExpr: {
    auto *param = expect<HIRParamPlaceExpr>(expr, HIRNodeKind::ParamPlaceExpr);

    if (param->param == nullptr) {
      return nullptr;
    }

    return param->param->symbol;
  }

  case HIRNodeKind::LoadExpr: {
    auto *load = expect<HIRLoadExpr>(expr, HIRNodeKind::LoadExpr);

    return getValueSymbol(load->place.get());
  }

  default:
    return nullptr;
  }
}

bool InitChecker::contain(const InitSet &set, ValueSymbol *symbol) {

  return set.find(symbol) != set.end();
}

void InitChecker::requireInitialized(ValueSymbol *symbol, HIRExpr *expr,
                                     const InitState &state) {

  if (symbol == nullptr) {
    Error::internal(expr->span, "InitChecker: ValueSymbol is nullptr");
  }

  if (contain(state.values, symbol)) {
    return;
  }

  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H006);

  dia.labels = {
      {expr->span, "'" + symbol->name + "' is used before it is initialized",
       true},
  };

  dia.notes = {
      "a value must be initialized on every reachable path before it is used",
  };

  dia.helps = {
      "initialize '" + symbol->name + "' before this use",
  };

  engine.emit(dia);
  recover.recover();
}

void InitChecker::requireField(ValueSymbol *owner, ValueSymbol *field,
                               HIRExpr *expr, const InitState &state) {

  if (owner == nullptr || field == nullptr) {
    Error::internal(expr->span, "InitChecker: field state has nullptr symbol");
  }

  auto it = state.fields.find(owner);

  if (it != state.fields.end() && contain(it->second, field)) {
    return;
  }

  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H006);

  dia.labels = {
      {expr->span,
       "field '" + field->name + "' is not initialized for '" + owner->name +
           "'",
       true},
  };

  dia.notes = {
      "this value was created through an initializer that does not guarantee "
      "this field",
  };

  dia.helps = {
      "use a field initialized by the selected initializer",
  };

  engine.emit(dia);
  recover.recover();
}

void InitChecker::requireSelfField(ValueSymbol *field, HIRExpr *expr) {

  if (field == nullptr) {
    Error::internal(expr->span, "InitChecker: self field is nullptr");
  }

  if (contain(currentSelfFields, field)) {
    return;
  }

  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H006);

  dia.labels = {
      {expr->span,
       "field '" + field->name + "' is used before it is initialized", true},
  };

  dia.notes = {
      "an initializer may only read fields initialized earlier on this "
      "control-flow path",
  };

  dia.helps = {
      "initialize '" + field->name + "' before this use",
  };

  engine.emit(dia);
  recover.recover();
}

InitSummary InitChecker::getSummary() const {
  InitSummary summary;

  summary.initFields = initFields;
  summary.commonFields = commonFields;

  return summary;
}