#pragma once

#include "hrd/SourceSpan.h"
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
  Case,
  OnExitStmt,
  ValueTransferStmt,
  DestroyStmt,
  QuitStmt,
  AssignStmt,
  CompoundAssignStmt,

  // expr
  LiteralExpr,
  LoadExpr,
  UnaryExpr,
  BinaryExpr,
  CastExpr,
  MethodCallExpr,
  RuntimeCallExpr,
  SpawnExpr,
  ViewExpr,
  MatchExpr,
  // EnumConstructExpr,
  TernaryExpr,
  DefaultValueExpr,

  // place
  LocalPlaceExpr,
  ParamPlaceExpr,
  FieldPlaceExpr,
  SelfExpr,
  // SuperExpr,
  RootExpr,
  ArrayAccessExpr,

  // value
  EnumVariantValue,
  StructInitExpr,
  // pattern
  LiteralPattern,
  EnumPattern,
  CasePattern,
};

struct HIRNode {
  HIRNodeKind kind;
  SourceSpan span;

  explicit HIRNode(SourceSpan s, HIRNodeKind k) : kind(k), span(s) {}
  virtual ~HIRNode() = default;
};
