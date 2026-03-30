/*
 * Copyright 2021 The Lynx Authors. All rights reserved.
 * Licensed under the Apache License Version 2.0 that can be found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <vector>

#include "ir/type.h"
#include "ir/value.h"

namespace wgx {
namespace ir {

/**
 * New Instruction Kinds (after value model unification)
 */
enum class InstKind {
  kReturn,     /** return [value] */
  kVariable,   /** var declaration: result_id (for var reference), type, name */
  kLoad,       /** load from variable: result_id, type, source_var_value */
  kStore,      /** store to variable: target_var_value, source_value */
  kBinary,     /** binary arithmetic: result_id, type, op, lhs, rhs */
  /** Future: kCall, kAccessChain, kConstruct, etc. */
};

enum class BinaryOpKind {
  kAdd,
  kSubtract,
  kMultiply,
  kDivide,
  /** etc. */
};

/**
 * New Unified Instruction Structure
 *
 * This replaces the old Instruction which had:
 * - result_id, result_type (used by different things in different ways)
 * - has_return_value, return_value_kind, const_vec4_f32, var_id, value_id
 * - binary_op (only valid for kBinary)
 * - operands (used differently depending on instruction)
 * - var_name (only valid for kVariable)
 *
 * New design principles:
 * 1. result_id is ONLY for instructions that produce SSA values
 * 2. All value operands use unified Value type
 * 3. No special case fields - everything goes through Value
 */
struct Instruction {
  InstKind kind = InstKind::kReturn;

  /**
   * Result ID - ONLY for instructions that produce an SSA value:
   * - kLoad: result of load
   * - kBinary: result of operation
   * NOT used by: kReturn, kVariable, kStore
   */
  uint32_t result_id = 0;

  /**
   * Result type - ONLY for instructions that produce values:
   * - kLoad: type of loaded value
   * - kBinary: type of result
   * NOT used by: kReturn, kVariable (type is in var declaration), kStore
   */
  TypeId result_type = kInvalidTypeId;

  /** For kVariable: variable name (for debugging) */
  std::string var_name;

  /** For kBinary: operation kind */
  BinaryOpKind binary_op = BinaryOpKind::kAdd;

  /**
   * Unified operands using Value type:
   * - kReturn: 0 or 1 operand (the return value as Value)
   * - kVariable: 0 or 1 operand (initializer value if present)
   * - kLoad: 1 operand (the variable Value to load from)
   * - kStore: 2 operands (target var Value, source Value)
   * - kBinary: 2 operands (lhs Value, rhs Value)
   */
  std::vector<Value> operands;

  /** ========================================================================
   *  Static factory methods for convenient construction
   *  ======================================================================== */

  /** Create a return void instruction */
  static Instruction ReturnVoid() {
    Instruction inst;
    inst.kind = InstKind::kReturn;
    return inst;
  }

  /** Create a return value instruction */
  static Instruction ReturnValue(const Value& value) {
    Instruction inst;
    inst.kind = InstKind::kReturn;
    inst.operands.push_back(value);
    return inst;
  }

  /**
   * Create a variable declaration (without initializer)
   *
   * Note: var_id is NOT stored in result_id - variables are addresses, not SSA values.
   * Instead, we create a Variable Value and store it in operands[0] or use a dedicated field.
   * For now, let's keep a separate var_id field for the declaration itself.
   * The Variable Value represents the address of this variable.
   */
  static Instruction VariableDecl(uint32_t var_id, TypeId var_type,
                                   const std::string& name) {
    Instruction inst;
    inst.kind = InstKind::kVariable;
    /** Reuse result_id for var_id in kVariable (it's not an SSA result) */
    inst.result_id = var_id;
    inst.result_type = var_type;
    inst.var_name = name;
    return inst;
  }

  /**
   * Create a load instruction: result = load(var)
   * Returns the new instruction which produces an SSA value.
   */
  static Instruction Load(uint32_t result_id, TypeId result_type,
                          const Value& var_ref) {
    Instruction inst;
    inst.kind = InstKind::kLoad;
    inst.result_id = result_id;       /** SSA result */
    inst.result_type = result_type;   /** Type of loaded value */
    inst.operands.push_back(var_ref); /** Source variable (must be Variable kind) */
    return inst;
  }

  /** Create a store instruction: store(target, source) */
  static Instruction Store(const Value& target_var, const Value& source_value) {
    Instruction inst;
    inst.kind = InstKind::kStore;
    inst.operands.push_back(target_var);   /** Must be Variable kind */
    inst.operands.push_back(source_value); /** Can be Constant or SSA */
    return inst;
  }

  /**
   * Create a binary operation: result = lhs op rhs
   */
  static Instruction Binary(uint32_t result_id, TypeId result_type,
                            BinaryOpKind op, const Value& lhs, const Value& rhs) {
    Instruction inst;
    inst.kind = InstKind::kBinary;
    inst.result_id = result_id;
    inst.result_type = result_type;
    inst.binary_op = op;
    inst.operands.push_back(lhs);
    inst.operands.push_back(rhs);
    return inst;
  }

  /** ========================================================================
   *  Helper accessors
   *  ======================================================================== */

  bool HasResult() const {
    return kind == InstKind::kLoad || kind == InstKind::kBinary;
    /** Note: kVariable doesn't produce SSA, it declares a storage location */
  }

  bool IsTerminator() const {
    return kind == InstKind::kReturn;
  }

  /**
   * Get the return value (for kReturn with value).
   * Returns nullptr if void return.
   */
  const Value* GetReturnValue() const {
    if (kind != InstKind::kReturn || operands.empty()) {
      return nullptr;
    }
    return &operands[0];
  }

  /** Get source variable for load */
  const Value& GetLoadSource() const {
    /** Assert: kind == kLoad */
    return operands[0];
  }

  /** Get store target and value */
  const Value& GetStoreTarget() const {
    /** Assert: kind == kStore */
    return operands[0];
  }
  const Value& GetStoreValue() const {
    /** Assert: kind == kStore */
    return operands[1];
  }

  /** Get binary operands */
  const Value& GetLhs() const {
    /** Assert: kind == kBinary */
    return operands[0];
  }
  const Value& GetRhs() const {
    /** Assert: kind == kBinary */
    return operands[1];
  }
};

/** Block remains similar but uses new Instruction */
struct Block {
  std::vector<Instruction> instructions;
};

/** Function updates to use unified id space */
struct Function {
  std::string name;
  PipelineStage stage = PipelineStage::kUnknown;
  bool return_builtin_position = false;
  Block entry_block;

  /**
   * Unified ID allocator - SSA values only.
   * Variable IDs come from a separate namespace (or we could unify them).
   */
  uint32_t next_ssa_id = 1;
  uint32_t next_var_id = 1;

  uint32_t AllocateSSAId() { return next_ssa_id++; }
  uint32_t AllocateVarId() { return next_var_id++; }
};

/** Module stays similar */
struct Module {
  std::string entry_point;
  PipelineStage stage = PipelineStage::kUnknown;
  std::vector<Function> functions;
  std::unique_ptr<TypeTable> type_table;
  std::unique_ptr<ConstantPool> constant_pool;
};

}  // namespace ir
}  // namespace wgx
