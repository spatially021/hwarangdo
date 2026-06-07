
#include "IR/HIR/HIRExpr.h"
#include "IR/MIR/MIRBuilder.h"
#include "IR/MIR/MIRExpr.h"
#include "util/Error.h"

unique_ptr<MIRExpr> MIRBuilder::lowerExpr(HIRExpr *expr) {
  switch (expr->kind) {

  case HIRNodeKind::LiteralExpr: {
    break;
  }
  case HIRNodeKind::LoadExpr: {
    break;
  }
  case HIRNodeKind::UnaryExpr: {
    break;
  }
  case HIRNodeKind::BinaryExpr: {
    break;
  }
  case HIRNodeKind::CastExpr: {
    break;
  }
  case HIRNodeKind::MethodCallExpr: {
    break;
  }
  case HIRNodeKind::SpawnExpr: {
    break;
  }
  case HIRNodeKind::ViewExpr: {
    break;
  }
  case HIRNodeKind::MatchExpr: {
    break;
  }
  case HIRNodeKind::TernaryExpr: {
    break;
  }
  case HIRNodeKind::LocalPlaceExpr: {
    break;
  }
  case HIRNodeKind::ParamPlaceExpr: {
    break;
  }
  case HIRNodeKind::FieldPlaceExpr: {
    break;
  }
  case HIRNodeKind::SelfExpr: {
    break;
  }
  case HIRNodeKind::RootExpr: {
    break;
  }
  case HIRNodeKind::ArrayAccessExpr: {
    break;
  }
  case HIRNodeKind::EnumVairantValue: {
    break;
  }
  case HIRNodeKind::StructInitExpr: {
    break;
  }
  case HIRNodeKind::LiteralPattern: {
    break;
  }
  case HIRNodeKind::EnumPattern: {
    break;
  }
  case HIRNodeKind::CasePattern: {
    break;
  }

  default: {
    break;
  }
  }

  Error::internal("ilegal hir kind");
}
