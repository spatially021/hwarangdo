#pragma once
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::unique_ptr;

enum class MirTypeKind {
  I32,
};

struct MirType {
  MirTypeKind kind;
};

struct MirValue {
  MirType type;
  virtual ~MirValue() = default;
};

struct ConstantIntValue : MirValue {
  int value;

  ConstantIntValue(int v) : value(v) { type.kind = MirTypeKind::I32; }
};

enum class BinaryOpKind {
  Add,
  Sub,
  Mul,
  Div,
  Rem,
  And,
  Or,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
};

struct BinaryOpValue : MirValue {
  BinaryOpKind op;
  MirValue *lhs;
  MirValue *rhs;

  BinaryOpValue(BinaryOpKind o, MirValue *l, MirValue *r)
      : op(o), lhs(l), rhs(r) {
    type.kind = MirTypeKind::I32;
  }
};

enum class TermKind {
  Return,
};

struct Terminator {
  TermKind kind;
  virtual ~Terminator() = default;
};

struct ReturnTerm : Terminator {
  MirValue *value;

  ReturnTerm(MirValue *v) : value(v) { kind = TermKind::Return; }
};

struct BasicBlock {
  vector<unique_ptr<MirValue>> values;
  unique_ptr<Terminator> terminator;
};

struct MirFunction {
  string name;
  MirType returnType;
  vector<unique_ptr<BasicBlock>> blocks;
};

struct MirModule {
  vector<unique_ptr<MirFunction>> functions;
};
