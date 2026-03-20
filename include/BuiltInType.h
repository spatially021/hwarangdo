#pragma once

enum class BuiltInType {
  I8,
  I16,
  I32,
  I64,
  I128,
  U8,
  U16,
  U32,
  U64,
  U128,
  F16,
  F32,
  F64,
  F128,
  C8,
  C16,
  C32,
  B,
  FI
};

enum class BuiltinCategory { Int, Float, Char, String, Bool, Void, Func };

struct BuiltinEntry {
  BuiltInType type;
  BuiltinCategory category;
  const char *name; // 심볼 이름 (필요하면 사용)
};

static constexpr BuiltinEntry builtinEntries[] = {

    // ---- Integer ----
    {BuiltInType::I8, BuiltinCategory::Int, "i8"},
    {BuiltInType::I16, BuiltinCategory::Int, "i16"},
    {BuiltInType::I32, BuiltinCategory::Int, "i32"},
    {BuiltInType::I64, BuiltinCategory::Int, "i64"},
    {BuiltInType::I128, BuiltinCategory::Int, "i128"},
    {BuiltInType::U8, BuiltinCategory::Int, "u8"},
    {BuiltInType::U16, BuiltinCategory::Int, "u16"},
    {BuiltInType::U32, BuiltinCategory::Int, "u32"},
    {BuiltInType::U64, BuiltinCategory::Int, "u64"},
    {BuiltInType::U128, BuiltinCategory::Int, "u128"},

    // ---- Float ----
    {BuiltInType::F16, BuiltinCategory::Float, "f16"},
    {BuiltInType::F32, BuiltinCategory::Float, "f32"},
    {BuiltInType::F64, BuiltinCategory::Float, "f64"},
    {BuiltInType::F128, BuiltinCategory::Float, "f128"},

    // ---- Char ----
    {BuiltInType::C8, BuiltinCategory::Char, "c8"},
    {BuiltInType::C16, BuiltinCategory::Char, "c16"},
    {BuiltInType::C32, BuiltinCategory::Char, "c32"},

    // ---- String ----
    {BuiltInType::C8, BuiltinCategory::String, "s8"},
    {BuiltInType::C16, BuiltinCategory::String, "s16"},
    {BuiltInType::C32, BuiltinCategory::String, "s32"},

    {BuiltInType::B, BuiltinCategory::Bool, "bool"},
};