#pragma once

#include "./Recover.h"

class SemanticAnalyzer;

class SemanticAnalyzerRecover final : public Recover {
public:
  SemanticAnalyzerRecover(SemanticAnalyzer &analyzer);
  ~SemanticAnalyzerRecover() {}
  void recover() override;

private:
  SemanticAnalyzer &analyzer;
};