/*
 * Copyright 2021 The Lynx Authors. All rights reserved.
 * Licensed under the Apache License Version 2.0 that can be found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "ir/type.h"

namespace wgx {
namespace ir {

/** Forward declarations */
class ConstantPool;

/**
 * ConstantKind - categories of inline constants that can be stored directly
 * in Value without needing a ConstantPool entry.
 */
enum class InlineConstKind {
  kNone,
  kF32,       /** 32-bit float */
  kI32,       /** 32-bit signed int */
  kU32,       /** 32-bit unsigned int */
  kBool,      /** boolean */
  kVec2F32,   /** vec2<f32> */
  kVec3F32,   /** vec3<f32> */
  kVec4F32,   /** vec4<f32> */
};

/**
 * ValueKind - discriminant for what a Value represents.
 *
 * This is the core of the unified value model:
 * - kConstant: Immediate constant value (inline or from constant pool)
 * - kSSA: Result of an instruction (SSA value)
 * - kVariable: Reference to a variable (address/lvalue)
 */
enum class ValueKind {
  kNone,
  kConstant,   /** Constant value - inline or from constant pool */
  kSSA,        /** SSA value (instruction result) */
  kVariable,   /** Variable reference (address, needs load to use as value) */
};

/**
 * Value - unified representation of any value-producing entity.
 *
 * Design principles:
 * 1. Always carries type information (no separate type lookup needed)
 * 2. Small and copyable (fits in a few registers)
 * 3. Constants can be inline (fast path) or pool-referenced (large constants)
 * 4. Explicit distinction between address (variable) and value (ssa/constant)
 *
 * Usage examples:
 *   // Constant vec4<f32>(1.0, 2.0, 3.0, 4.0)
 *   Value c = Value::ConstantVec4F32(type_id, 1.0f, 2.0f, 3.0f, 4.0f);
 *
 *   // SSA value from binary operation result
 *   Value ssa = Value::SSA(type_id, result_id);
 *
 *   // Variable reference (address)
 *   Value var = Value::Variable(var_type_id, var_id);
 *
 *   // Load: convert variable (address) to SSA (value)
 *   // This is done by creating a kLoad instruction that produces an SSA Value
 */
struct Value {
  /** ========================================================================
   *  Static factory methods
   *  ======================================================================== */

  /** Create an invalid/null value */
  static Value None();

  /** Create an inline f32 constant */
  static Value ConstantF32(TypeId type, float value);

  /** Create an inline i32 constant */
  static Value ConstantI32(TypeId type, int32_t value);

  /** Create an inline u32 constant */
  static Value ConstantU32(TypeId type, uint32_t value);

  /** Create an inline bool constant */
  static Value ConstantBool(TypeId type, bool value);

  /** Create an inline vec2<f32> constant */
  static Value ConstantVec2F32(TypeId type, float x, float y);

  /** Create an inline vec3<f32> constant */
  static Value ConstantVec3F32(TypeId type, float x, float y, float z);

  /** Create an inline vec4<f32> constant */
  static Value ConstantVec4F32(TypeId type, float x, float y, float z, float w);

  /** Create an inline vec4<f32> constant from array */
  static Value ConstantVec4F32(TypeId type, const std::array<float, 4>& values);

  /**
   * Create a constant referencing the constant pool (for large/complex constants).
   * The pool_index is an index into ConstantPool.
   */
  static Value ConstantPoolRef(TypeId type, uint32_t pool_index);

  /** Create an SSA value (instruction result) */
  static Value SSA(TypeId type, uint32_t ssa_id);

  /** Create a variable reference (address/lvalue) */
  static Value Variable(TypeId var_type, uint32_t var_id);

  /** ========================================================================
   *  Member data
   *  ======================================================================== */

  ValueKind kind = ValueKind::kNone;
  TypeId type = kInvalidTypeId;

  /**
   * For kSSA: the SSA value id (result of an instruction)
   * For kVariable: the variable id
   * For kConstant with pool reference: pool index
   */
  uint32_t id = 0;

  /** For kConstant: what kind of inline constant (if any) */
  InlineConstKind const_kind = InlineConstKind::kNone;

  /** Constant data storage (union for space efficiency) */
  union ConstData {
    float f32;
    int32_t i32;
    uint32_t u32;
    bool boolean;
    std::array<float, 4> vec4;
    uint32_t pool_index;

    ConstData() : u32(0) {}
    ~ConstData() = default;
  } const_data;

  /** ========================================================================
   *  Inline convenience accessors and queries
   *  ======================================================================== */

  bool IsValid() const { return kind != ValueKind::kNone && type != kInvalidTypeId; }
  bool IsConstant() const { return kind == ValueKind::kConstant; }
  bool IsSSA() const { return kind == ValueKind::kSSA; }
  bool IsVariable() const { return kind == ValueKind::kVariable; }

  /** Check if this is an address/lvalue (variable reference) */
  bool IsAddress() const { return kind == ValueKind::kVariable; }

  /** Check if this is a value (constant or SSA) */
  bool IsValue() const { return kind == ValueKind::kConstant || kind == ValueKind::kSSA; }

  /** Check if constant is stored inline (vs in constant pool) */
  bool IsInlineConstant() const {
    return kind == ValueKind::kConstant && const_kind != InlineConstKind::kNone;
  }

  /** ========================================================================
   *  Safe value extraction - returns nullopt/std::array{} if type mismatch
   *  ======================================================================== */

  /** Get the SSA id if this is an SSA value, otherwise nullopt */
  std::optional<uint32_t> GetSSAId() const;

  /** Get the variable id if this is a variable reference, otherwise nullopt */
  std::optional<uint32_t> GetVarId() const;

  /** Get inline vec4<f32> values if this is a vec4<f32> constant, otherwise empty array */
  std::array<float, 4> GetVec4F32() const;

  /** Get inline f32 value if this is an f32 constant, otherwise 0.0f */
  float GetF32() const;

  /** Get inline i32 value if this is an i32 constant, otherwise 0 */
  int32_t GetI32() const;

  /** Get inline u32 value if this is a u32 constant, otherwise 0 */
  uint32_t GetU32() const;

  /** Get inline bool value if this is a bool constant, otherwise false */
  bool GetBool() const;

  /** Get constant pool index if this is a pool reference, otherwise nullopt */
  std::optional<uint32_t> GetPoolIndex() const;

  /** ========================================================================
   *  Unchecked value extraction - caller must verify type first
   *  These are faster but unsafe if called on wrong Value kind
   *  ======================================================================== */

  /** Directly return the id field - works for SSA, Variable, and PoolRef */
  uint32_t GetIdUnchecked() const { return id; }

  /** Directly access the f32 field in const_data */
  float GetF32Unchecked() const { return const_data.f32; }

  /** Directly access the vec4 field in const_data */
  const std::array<float, 4>& GetVec4Unchecked() const { return const_data.vec4; }

  /** Directly access the i32 field in const_data */
  int32_t GetI32Unchecked() const { return const_data.i32; }

  /** Directly access the u32 field in const_data */
  uint32_t GetU32Unchecked() const { return const_data.u32; }

  /** Directly access the boolean field in const_data */
  bool GetBoolUnchecked() const { return const_data.boolean; }

  /** Directly access the pool_index field in const_data */
  uint32_t GetPoolIndexUnchecked() const { return const_data.pool_index; }
};

/**
 * ExprResult - Result of lowering an expression
 *
 * This replaces using Instruction* as a temporary carrier during lowering.
 * It clearly separates:
 * - The value produced by the expression (Value)
 * - Whether the expression result is an address (lvalue) or value (rvalue)
 */
struct ExprResult {
  /** The value produced (may be address or value depending on context) */
  Value value;

  /**
   * True if this is an address/lvalue (can be assigned to, needs load for value)
   * False if this is already a value (constant or SSA)
   */
  bool is_address = false;

  /** Static factory methods (inline) */
  static ExprResult ValueResult(const Value& v) {
    ExprResult r;
    r.value = v;
    r.is_address = false;
    return r;
  }

  static ExprResult AddressResult(const Value& var_ref) {
    ExprResult r;
    r.value = var_ref;
    r.is_address = true;
    return r;
  }

  /** Convenience accessors (inline) */
  bool IsAddress() const { return is_address; }
  bool IsValue() const { return !is_address; }
  bool IsValid() const { return value.IsValid(); }
  TypeId GetType() const { return value.type; }

  /**
   * Convert to Value (for use as rvalue).
   * If this is an address, you need to emit a Load instruction first.
   */
  const Value& AsValue() const { return value; }
};

/**
 * ConstantPool - Storage for complex/large constants
 *
 * For constants that don't fit in Value's inline storage (e.g., large arrays,
 * matrices, structs), we store them in a pool and reference by index.
 */
class ConstantPool {
 public:
  ConstantPool();
  ~ConstantPool();

  /** Prevent copy/move */
  ConstantPool(const ConstantPool&) = delete;
  ConstantPool& operator=(const ConstantPool&) = delete;

  /**
   * Add a constant and return its pool index.
   * TODO: Implement when needed for arrays, matrices, etc.
   */

  /**
   * Lookup constant by index.
   * TODO: Implement when needed
   */

 private:
  /** TODO: Storage for complex constants */
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ir
}  // namespace wgx
