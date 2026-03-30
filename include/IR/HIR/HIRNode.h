#pragma once

struct SourceSpan {
  int lineStart = 0;
  int colStart = 0;
  int lineEnd = 0;
  int colEnd = 0;
};

enum class HIRNodeKind {
  Program,
  Source,

  // decl
  TypeDecl,
  MethodDecl,
  FieldDecl,
  ParamDecl,

  // stmt
  BlockStmt,
  ExprStmt,
  LocalDeclStmt,
  MethodDeclStmt,
  IfStmt,
  WhileStmt,
  ForRangeStmt,
  ReturnStmt,
  BreakStmt,
  ContinueStmt,
  SwitchStmt,
  OnExitStmt,

  // expr
  LiteralExpr,
  LoadExpr,
  AssignExpr,
  UnaryExpr,
  BinaryExpr,
  CastExpr,
  CallExpr,
  MethodCallExpr,
  SpawnExpr,
  ViewExpr,
  MatchExpr,
  EnumConstructExpr,
  TernaryExpr,

  // place
  LocalPlaceExpr,
  ParamPlaceExpr,
  FieldPlaceExpr,
  SelfExpr,
  SuperExpr,
  RootExpr,
  TempPlaceExpr,
  ArrayAccessExpr,

  // value
  EnumVairantValue,

  // pattern
  LiteralPattern,
  EnumPattern,
  WildcardPattern,
};

struct HIRNode {
  HIRNodeKind kind;
  SourceSpan span;

  explicit HIRNode(HIRNodeKind k, SourceSpan s = {}) : kind(k), span(s) {}
  virtual ~HIRNode() = default;
};
