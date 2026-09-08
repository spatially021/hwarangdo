
#include "hrd/AST/Decl.h"
#include "hrd/SourceSpan.h"
#include "hrd/enums/Casting.h"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <string>
#include <vector>

class MethodSymbol;
class TraitSig;
class SymbolTable;

struct SigResult {
  bool result;
  SourceSpan span;
  MethodSymbol *symbol;
};

namespace Helper {
std::string apIntToString(const llvm::APInt &v);
pair<bool, SourceSpan> hasSameSig(const std::vector<MethodSymbol *> &vec,
                                  MethodSymbol *method);
bool hasSameSig(const vector<TraitSig *> &vec, TraitSig *sig);
pair<bool, TypeSymbol *> checkImplementTraitSig(ObjectType *symbol);
SigResult hasSameMethodSig(const std::vector<MethodSymbol *> &vec,
                           MethodSymbol *method);
bool hasMethodInHierarchyWithSameSig(ObjectType *type, const std::string &name,
                                     MethodSymbol *sig);
pair<bool, CastingResultKind> canImplicitlyConvert(TypeSymbol *from,
                                                   TypeSymbol *to);
} // namespace Helper