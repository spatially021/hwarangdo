#pragma once
#include <string>
#include <vector>
struct InputSource {
  std::string path;
  std::string text;
};

struct ProjectInput {
  std::string rootPath;
  std::string projectFilePath;
  std::string srcPath;
  std::vector<InputSource> sources;
};
