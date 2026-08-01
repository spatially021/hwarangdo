#pragma once

#include "./Recover.h"

class Parser;

enum class ParserRecoveryPoint {
  Declaration,
  TypeMember,
  Statement,
  ParameterList,
  ArgumentList,
};

class ParserRecover final : public Recover {
public:
  ParserRecover(Parser &parser);
  ~ParserRecover() {}
  void recover() override;

private:
  Parser &parser;
};