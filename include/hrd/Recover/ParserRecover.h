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
  void recover(ParserRecoveryPoint point);

private:
  Parser &parser;
};