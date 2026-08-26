#include "hrd/MetaData/MetaData.h"

struct ImportedModule {
  ModuleMeta meta;
  std::filesystem::path objectPath;
};