
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <string>

namespace Helper {
std::string apIntToString(const llvm::APInt &v);
} // namespace Helper