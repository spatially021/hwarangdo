#pragma once

struct SourceSpan {
  int lineStart = 0;
  int colStart = 0;
  int lineEnd = 0;
  int colEnd = 0;
};

enum class HIRNodeKind {
  Module,

  // decl
  TypeDecl,
  MethodDecl,
  FieldDecl,
  ParamDecl,

  // stmt
  BlockStmt,
  ExprStmt,
  LocalDeclStmt,
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

  // place
  LocalPlaceExpr,
  ParamPlaceExpr,
  FieldPlaceExpr,
  ThisPlaceExpr,
  SuperPlaceExpr,
  TempPlaceExpr,

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
