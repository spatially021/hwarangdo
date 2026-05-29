#pragma once

#include "SourceSpan.h"
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

  // expr
  LiteralExpr,
  LoadExpr,
  AssignExpr,
  CompoundAssignExpr,
  UnaryExpr,
  BinaryExpr,
  CastExpr,
  MethodCallExpr,
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
  TempPlaceExpr,
  ArrayAccessExpr,

  // value
  EnumVairantValue,
  StructInitExpr,
  // pattern
  LiteralPattern,
  EnumPattern,
  CasePattern,
  WildcardValue,
};

struct HIRNode {
  HIRNodeKind kind;
  SourceSpan span;

  explicit HIRNode(SourceSpan s, HIRNodeKind k) : kind(k), span(s) {}
  virtual ~HIRNode() = default;
};
