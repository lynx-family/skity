// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ir/type.h"
#include "ir/value.h"

namespace wgx {
namespace ir {

enum class PipelineStage {
  kUnknown,
  kVertex,
  kFragment,
};

enum class InstKind {
  kReturn,
  kVariable,
  kLoad,
  kStore,
  kBinary,
};

// Supported binary operations in the current IR subset.
enum class BinaryOpKind {
  kAdd,
  kSubtract,
};

struct Instruction {
  InstKind kind = InstKind::kReturn;

  // Result SSA id for instructions that produce a value.
  uint32_t result_id = 0;

  // Result type for value-producing instructions, or declared type for
  // kVariable.
  TypeId result_type = kInvalidTypeId;

  // Variable id for kVariable declarations.
  uint32_t var_id = 0;

  // Variable name (for debugging)
  std::string var_name;

  // Binary op kind for kBinary instructions
  BinaryOpKind binary_op = BinaryOpKind::kAdd;

  // Unified operands expressed via Value.
  // - kReturn: 0 or 1 operand (return value)
  // - kVariable: 0 operands today (initializer lowered as a following kStore)
  // - kLoad: 1 operand (source variable address)
  // - kStore: 2 operands (target variable address, source value)
  // - kBinary: 2 operands (lhs value, rhs value)
  std::vector<Value> operands = {};

  bool HasResult() const {
    return kind == InstKind::kLoad || kind == InstKind::kBinary;
  }
};

struct Block {
  std::vector<Instruction> instructions = {};
};

struct Function {
  std::string name = {};
  PipelineStage stage = PipelineStage::kUnknown;
  bool return_builtin_position = false;
  Block entry_block = {};

  // Variable/value id allocator for this function
  uint32_t next_var_id = 1;
  uint32_t next_ssa_id = 1;

  uint32_t AllocateVarId() { return next_var_id++; }
  uint32_t AllocateSSAId() { return next_ssa_id++; }
};

struct Module {
  std::string entry_point = {};
  PipelineStage stage = PipelineStage::kUnknown;
  std::vector<Function> functions = {};

  // Module-level type table (shared across functions)
  std::unique_ptr<TypeTable> type_table;
  std::unique_ptr<ConstantPool> constant_pool;
};

}  // namespace ir
}  // namespace wgx
