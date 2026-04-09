// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
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

using BlockId = uint32_t;
constexpr BlockId kInvalidBlockId = 0;

// Output variable decoration types
enum class OutputDecorationKind {
  kNone,      // No decoration (void)
  kBuiltin,   // @builtin(...)
  kLocation,  // @location(...)
};

// Backend-agnostic builtin types
// These map to backend-specific values (e.g., SpvBuiltInPosition in SPIR-V)
enum class BuiltinType {
  kNone,
  kPosition,
  // Future: kFragDepth, kSampleMask, kVertexIndex, etc.
};

// Description of a single output variable
// For struct returns, each member becomes one OutputVariable
struct OutputVariable {
  // Member name in the struct (or empty for simple returns)
  std::string name;

  // Type of this output variable
  TypeId type = kInvalidTypeId;

  // Decoration kind
  OutputDecorationKind decoration_kind = OutputDecorationKind::kNone;

  // Decoration value:
  // - For kBuiltin: BuiltinType enum value
  // - For kLocation: location index (0, 1, 2, ...)
  uint32_t decoration_value = 0;

  // Helper to set builtin decoration
  void SetBuiltin(BuiltinType builtin) {
    decoration_kind = OutputDecorationKind::kBuiltin;
    decoration_value = static_cast<uint32_t>(builtin);
  }

  // Helper to get builtin type
  BuiltinType GetBuiltin() const {
    if (decoration_kind == OutputDecorationKind::kBuiltin) {
      return static_cast<BuiltinType>(decoration_value);
    }
    return BuiltinType::kNone;
  }

  // Helper to set location decoration
  void SetLocation(uint32_t loc) {
    decoration_kind = OutputDecorationKind::kLocation;
    decoration_value = loc;
  }

  // Helper to get location index
  uint32_t GetLocation() const {
    if (decoration_kind == OutputDecorationKind::kLocation) {
      return decoration_value;
    }
    return 0;
  }
};

enum class InstKind {
  kReturn,
  kVariable,
  kLoad,
  kStore,
  kBinary,
  kBranch,
  kCondBranch,
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

  // Branch targets for control-flow terminators.
  BlockId target_block = kInvalidBlockId;
  BlockId true_block = kInvalidBlockId;
  BlockId false_block = kInvalidBlockId;
  BlockId merge_block = kInvalidBlockId;

  // Unified operands expressed via Value.
  // - kReturn: 0 or 1 operand (return value)
  // - kVariable: 0 operands today (initializer lowered as a following kStore)
  // - kLoad: 1 operand (source variable address)
  // - kStore: 2 operands (target variable address, source value)
  // - kBinary: 2 operands (lhs value, rhs value)
  // - kCondBranch: 1 operand (boolean condition value)
  std::vector<Value> operands = {};

  bool HasResult() const {
    return kind == InstKind::kLoad || kind == InstKind::kBinary;
  }

  bool IsTerminator() const {
    return kind == InstKind::kReturn || kind == InstKind::kBranch ||
           kind == InstKind::kCondBranch;
  }
};

struct Block {
  BlockId id = kInvalidBlockId;
  std::string name = {};
  BlockId loop_merge_block = kInvalidBlockId;
  BlockId loop_continue_block = kInvalidBlockId;
  std::vector<Instruction> instructions = {};

  bool IsLoopHeader() const {
    return loop_merge_block != kInvalidBlockId &&
           loop_continue_block != kInvalidBlockId;
  }
};

struct Function {
  std::string name = {};
  PipelineStage stage = PipelineStage::kUnknown;

  // Return type (kInvalidTypeId = void)
  // For struct returns, this is the struct type
  TypeId return_type = kInvalidTypeId;

  // Output variables for entry point interface
  // - Empty for void returns
  // - One entry for simple scalar/vector returns
  // - Multiple entries for struct returns (one per decorated member)
  std::vector<OutputVariable> output_vars = {};

  BlockId entry_block_id = kInvalidBlockId;
  std::vector<Block> blocks = {};

  // Variable/value id allocator for this function
  uint32_t next_var_id = 1;
  uint32_t next_ssa_id = 1;
  BlockId next_block_id = 1;

  uint32_t AllocateVarId() { return next_var_id++; }
  uint32_t AllocateSSAId() { return next_ssa_id++; }
  BlockId AllocateBlockId() { return next_block_id++; }

  Block* GetBlock(BlockId id) {
    for (auto& block : blocks) {
      if (block.id == id) {
        return &block;
      }
    }
    return nullptr;
  }

  const Block* GetBlock(BlockId id) const {
    for (const auto& block : blocks) {
      if (block.id == id) {
        return &block;
      }
    }
    return nullptr;
  }
};

struct Module {
  std::string entry_point = {};
  PipelineStage stage = PipelineStage::kUnknown;
  std::vector<Function> functions = {};

  // Module-level type table (shared across functions)
  std::unique_ptr<TypeTable> type_table;
  std::unique_ptr<ConstantPool> constant_pool;

  /**
   * Global variable metadata.
   * Key: variable id (as used in Value::Variable)
   *
   * Global variables are referenced by Load/Store instructions but not declared
   * as kVariable instructions. Their initializers must be constant expressions.
   */
  struct GlobalVariable {
    TypeId type = kInvalidTypeId;
    StorageClass storage_class = StorageClass::kPrivate;
    std::optional<Value> initializer;
    std::optional<uint32_t> group;
    std::optional<uint32_t> binding;

    /**
     * For uniform/storage buffers that need struct wrapping.
     * In Vulkan SPIR-V, Uniform/StorageBuffer storage class must be structs.
     * If this is set, the actual value type is different from the declared
     * type.
     */
    TypeId inner_type = kInvalidTypeId;
  };
  std::unordered_map<uint32_t, GlobalVariable> global_variables;
};

}  // namespace ir
}  // namespace wgx
