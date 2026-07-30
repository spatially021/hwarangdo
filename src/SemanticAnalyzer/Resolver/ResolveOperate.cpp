#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"

void Resolver::visit(UnaryExpr *expr) {
  expr->right->accept(this);

  if (expr->tOp.kind == TKind::BANG) {
    if (table.isBool(expr->right->resolvedType)) {
      expr->resolvedType = expr->right->resolvedType;
      expr->op = Operator::L_NOT;
    } else if (table.isInt(expr->right->resolvedType)) {
      expr->resolvedType = expr->right->resolvedType;
      expr->op = Operator::B_NOT;
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S066);
      dia.labels = {
          {expr->right->span,
           "type '" + expr->right->resolvedType->name +
               "' does not support unary operator '!'",
           true},
      };
      dia.notes = {
          "operator '!' performs logical negation on bool values and bitwise "
          "negation on integer values",
      };
      dia.helps = {
          "use a bool or integer operand",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else if (expr->tOp.kind == TKind::PLUS || expr->tOp.kind == TKind::MINUS) {
    if (table.isNumberic(expr->right->resolvedType)) {
      expr->resolvedType = expr->right->resolvedType;
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S066);
      dia.labels = {
          {expr->right->span,
           "type '" + expr->right->resolvedType->name +
               "' does not support unary operator '" + expr->tOp.text + "'",
           true},
      };
      dia.notes = {
          "unary operator '" + expr->tOp.text + "' requires a numeric operand",
      };
      dia.helps = {
          "use an integer or floating-point operand",
      };
      engine.emit(dia);
      recover.recover();
    }

    if (expr->tOp.kind == TKind::PLUS) {
      expr->op = Operator::PLUS;
    } else {
      expr->op = Operator::MINUS;

      if (!table.isSigned(expr->right->resolvedType)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S067);
        dia.labels = {
            {expr->right->span,
             "type '" + expr->right->resolvedType->name +
                 "' cannot represent a negative value",
             true},
        };
        dia.notes = {
            "unary operator '-' is not supported for unsigned numeric types",
        };
        dia.helps = {
            "use a signed numeric type",
        };
        engine.emit(dia);
        recover.recover();
      }
    }
  }
}

void Resolver::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);

  if (expr->left->resolvedType == nullptr) {
    Error::internal(expr->left->span, "lhs is nullptr");
  }

  if (expr->right->resolvedType == nullptr) {
    Error::internal(expr->right->span, "rhs is nullptr");
  }

  if (isBinaryOperatalbe(expr->op, expr->left->resolvedType,
                         expr->right->resolvedType)) {
    auto operandType =
        binaryCasting(expr->left->resolvedType, expr->right->resolvedType);

    auto result = binaryResult(expr->op, expr->left->resolvedType,
                               expr->right->resolvedType);

    if (!result.first) {
      Error::internal(expr->span, "fail to get binaryResult");
    }

    if (operandType.first == nullptr) {
      Error::internal("fail to get operand type");
    }

    if (result.second == CastingResultKind::PrecisionLoss) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S069);
      dia.labels = {
          {expr->span,
           "this operation implicitly converts between '" +
               expr->left->resolvedType->name + "' and '" +
               expr->right->resolvedType->name + "'",
           true},
      };
      dia.notes = {
          "the implicit conversion may lose numeric precision",
      };
      dia.helps = {
          "use an explicit cast to acknowledge the possible precision loss",
      };
      engine.emit(dia);
    }

    expr->resolvedType = result.first;
    expr->operrandType = operandType.first;
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S068);
    dia.labels = {
        {expr->left->span,
         "left operand has type '" + expr->left->resolvedType->name + "'",
         true},
        {expr->right->span,
         "right operand has type '" + expr->right->resolvedType->name + "'",
         false},
    };
    dia.notes = {
        "operator '" + expr->opRaw.text +
            "' is not defined for these operand types",
    };
    dia.helps = {
        "use operands with compatible types",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (!expr->resolvedType) {
    Error::internal(expr->span, "unresolved type");
  }
}

void Resolver::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);

  if (expr->conditon->resolvedType != table.getBool()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S070);
    dia.labels = {
        {expr->conditon->span,
         "condition has type '" + expr->conditon->resolvedType->name + "'",
         true},
    };
    dia.notes = {
        "a ternary condition must have type 'bool'",
    };
    dia.helps = {
        "use a boolean expression as the condition",
    };
    engine.emit(dia);
    recover.recover();
  }

  expr->then->accept(this);
  expr->else_->accept(this);

  if (!isCastable(expr->then->resolvedType, expr->else_->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S071);
    dia.labels = {
        {expr->then->span,
         "then expression has type '" + expr->then->resolvedType->name + "'",
         true},
        {expr->else_->span,
         "else expression has type '" + expr->else_->resolvedType->name + "'",
         false},
    };
    dia.notes = {
        "both branches of a ternary expression must have compatible types",
    };
    dia.helps = {
        "convert one branch to a type compatible with the other branch",
    };
    engine.emit(dia);
    recover.recover();
  }

  // TODO: 삼항 연산의 최종 타입 결정 로직 추가
  expr->resolvedType = expr->then->resolvedType;
}