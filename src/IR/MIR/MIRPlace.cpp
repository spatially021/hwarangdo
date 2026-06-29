
#include "hrd/AST/Expr.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/util/Error.h"
#include <memory>
#include <utility>

unique_ptr<MIRPlace> MIRBuilder::lowerPlace(HIRPlaceExpr *expr) {
  if (auto local = dynamic_cast<HIRLocalPlaceExpr *>(expr)) {
    return lowerLocal(local);
  }

  if (auto param = dynamic_cast<HIRParamPlaceExpr *>(expr)) {
    return lowerParam(param);
  }

  if (auto array = dynamic_cast<HIRArrayAccessPlaceExpr *>(expr)) {
    return lowerArray(array);
  }

  if (auto field = dynamic_cast<HIRFieldPlaceExpr *>(expr)) {
    return lowerField(field);
  }

  Error::internal(expr->span, "illegal place kind");
}

unique_ptr<MIRLocalPlace> MIRBuilder::lowerLocal(HIRLocalPlaceExpr *expr) {
  return make_unique<MIRLocalPlace>(expr->local->symbol);
}

unique_ptr<MIRParamPlace> MIRBuilder::lowerParam(HIRParamPlaceExpr *expr) {
  return make_unique<MIRParamPlace>(expr->param->symbol);
}

unique_ptr<MIRArrayAccessPlace>
MIRBuilder::lowerArray(HIRArrayAccessPlaceExpr *expr) {
  unique_ptr<MIRPlace> base = lowerPlace(expr->object.get());
  unique_ptr<MIRValue> index = lowerExpr(expr->index.get());
  return make_unique<MIRArrayAccessPlace>(std::move(base), std::move(index));
}

unique_ptr<MIRFieldPlace> MIRBuilder::lowerField(HIRFieldPlaceExpr *expr) {

  return make_unique<MIRFieldPlace>(expr->field->symbol,
                                    lowerReceiverToPlace(expr->receiver.get()));
}

unique_ptr<MIRPlace> MIRBuilder::lowerReceiverToPlace(HIRExpr *receiver) {
  if (dynamic_cast<HIRSelfExpr *>(receiver)) {
    return make_unique<MIRParamPlace>(currentFunc->symbol->selfReceiver);
  }

  if (dynamic_cast<HIRRootExpr *>(receiver)) {
    return make_unique<MIRRootPlace>();
  }

  if (auto load = dynamic_cast<HIRLoadExpr *>(receiver)) {
    return lowerPlace(load->place.get());
  }

  if (auto place = dynamic_cast<HIRPlaceExpr *>(receiver)) {
    return lowerPlace(place);
  }

  Error::internal(receiver->span, "illegal receiver kind");
}
