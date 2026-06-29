
#include "hrd/AST/Decl.h"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <string>
#include <vector>

class MethodSymbol;
class TraitSig;

namespace Helper {
std::string apIntToString(const llvm::APInt &v);
bool hasSameSig(const std::vector<MethodSymbol *> &vec, MethodSymbol *method);
bool hasSameSig(const vector<TraitSig *> &vec, TraitSig *sig);
bool checkImplementTraitSig(TypeSymbol *symbol);
bool hasSameMethodSig(const std::vector<MethodSymbol *> &vec,
                      MethodSymbol *method);
bool hasMethodInHierarchyWithSameSig(TypeSymbol *type, const std::string &name,
                                     MethodSymbol *sig);
} // namespace Helper