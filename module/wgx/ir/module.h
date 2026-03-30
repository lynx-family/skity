// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ir/type.h"

namespace wgx {
namespace ir {

enum class PipelineStage {
  kUnknown,
  kVertex,
  kFragment,
};

enum class InstKind {
  kReturn,
  kVariable,   // var declaration: result_id, type
  kLoad,       // load from variable: result_id, type, var_id
  kStore,      // store to variable: var_id, value_id
  kBinary,     // binary arithmetic producing an SSA value
};

enum class ReturnValueKind {
  kNone,
  kConstVec4F32,
  kVariableRef,   // return a variable reference (loaded)
  kValueRef,      // return an SSA value directly
};

// Supported binary operations in the current IR subset.
enum class BinaryOpKind {
  kAdd,
  kSubtract,
};

// Operand for IR instructions
struct Operand {
  enum class Kind {
    kNone,
    kId,       // SSA value id
    kConstF32, // float constant
  };

  Kind kind = Kind::kNone;
  uint32_t id = 0;           // for kId
  float const_f32 = 0.0f;    // for kConstF32

  static Operand Id(uint32_t id) {
    Operand op;
    op.kind = Kind::kId;
    op.id = id;
    return op;
  }

  static Operand ConstF32(float value) {
    Operand op;
    op.kind = Kind::kConstF32;
    op.const_f32 = value;
    return op;
  }
};

struct Instruction {
  InstKind kind = InstKind::kReturn;

  // Result value id (if instruction produces a value)
  uint32_t result_id = 0;

  // Type of the result value (TypeId from TypeTable)
  TypeId result_type = kInvalidTypeId;

  // For return instruction
  bool has_return_value = false;
  ReturnValueKind return_value_kind = ReturnValueKind::kNone;
  std::array<float, 4> const_vec4_f32 = {0.f, 0.f, 0.f, 0.f};

  // Variable reference for kVariable/kLoad return value
  uint32_t var_id = 0;

  // SSA value reference for return / value-producing instructions
  uint32_t value_id = 0;

  // Binary op kind for kBinary instructions
  BinaryOpKind binary_op = BinaryOpKind::kAdd;

  // Operands for variable/load/store/binary instructions
  std::vector<Operand> operands = {};

  // Variable name (for debugging)
  std::string var_name;
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
  uint32_t next_value_id = 1;

  uint32_t AllocateVarId() { return next_var_id++; }
  uint32_t AllocateValueId() { return next_value_id++; }
};

struct Module {
  std::string entry_point = {};
  PipelineStage stage = PipelineStage::kUnknown;
  std::vector<Function> functions = {};

  // Module-level type table (shared across functions)
  std::unique_ptr<TypeTable> type_table;
};

}  // namespace ir
}  // namespace wgx
