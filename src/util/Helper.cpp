#include "util/Helper.h"

std::string Helper::apIntToString(const llvm::APInt &v) {
  llvm::SmallString<32> buf;
  v.toString(buf, 10, false);
  return std::string(buf.str());
}