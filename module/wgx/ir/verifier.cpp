// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ir/verifier.h"

#include <algorithm>

namespace wgx {
namespace ir {

// =============================================================================
// Public API
// =============================================================================

VerificationResult Verifier::VerifyModule(const Module& module) {
  for (size_t i = 0; i < module.functions.size(); ++i) {
    auto result = VerifyFunction(module.functions[i]);
    if (!result.valid) {
      return result;
    }
  }
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyFunction(const Function& function) {
  ResetState();

  // Check function has a valid name
  if (function.name.empty()) {
    return VerificationResult::Failure("Function has no name");
  }

  // Check entry block exists (even if empty, that's a separate check)
  // Single-block validation for now
  const Block& block = function.entry_block;

  // Verify each instruction
  for (size_t i = 0; i < block.instructions.size(); ++i) {
    auto result = VerifyInstruction(block.instructions[i], i, function);
    if (!result.valid) {
      return result;
    }
  }

  // Check block terminator rules
  if (block.instructions.empty()) {
    return VerificationResult::Failure("Entry block has no instructions");
  }

  const auto& last_inst = block.instructions.back();
  if (last_inst.kind != InstKind::kReturn) {
    return VerificationResult::Failure(
        "Entry block does not end with return instruction",
        block.instructions.size() - 1,
        last_inst.kind);
  }

  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyInstruction(const Instruction& inst,
                                                size_t index,
                                                const Function& function) {
  (void)function;  // Reserved for future use (e.g., dominator checks)

  switch (inst.kind) {
    case InstKind::kReturn:
      return VerifyReturn(inst, index);
    case InstKind::kVariable:
      return VerifyVariable(inst, index);
    case InstKind::kLoad:
      return VerifyLoad(inst, index);
    case InstKind::kStore:
      return VerifyStore(inst, index);
    case InstKind::kBinary:
      return VerifyBinary(inst, index);
    default:
      return VerificationResult::Failure(
          "Unknown instruction kind", index, inst.kind);
  }
}

// =============================================================================
// Private Implementation
// =============================================================================

void Verifier::ResetState() {
  defined_ssa_ids_.clear();
  defined_var_ids_.clear();
}

void Verifier::TrackSSADefinition(uint32_t ssa_id) {
  if (ssa_id != 0) {
    defined_ssa_ids_.push_back(ssa_id);
  }
}

void Verifier::TrackVariableDefinition(uint32_t var_id) {
  if (var_id != 0) {
    defined_var_ids_.push_back(var_id);
  }
}

bool Verifier::IsSSADefined(uint32_t ssa_id) const {
  if (ssa_id == 0) return false;
  return std::find(defined_ssa_ids_.begin(), defined_ssa_ids_.end(),
                   ssa_id) != defined_ssa_ids_.end();
}

bool Verifier::IsVariableDefined(uint32_t var_id) const {
  if (var_id == 0) return false;
  return std::find(defined_var_ids_.begin(), defined_var_ids_.end(),
                   var_id) != defined_var_ids_.end();
}

bool Verifier::IsValidValue(const Value& value,
                            const std::string& context) const {
  if (!value.IsValid()) {
    return false;
  }

  // For SSA values, check they've been defined
  if (value.IsSSA()) {
    auto ssa_id = value.GetSSAId();
    if (!ssa_id.has_value()) {
      return false;
    }
    if (!IsSSADefined(ssa_id.value())) {
      return false;
    }
  }

  // For variable references, check they've been defined
  if (value.IsVariable()) {
    auto var_id = value.GetVarId();
    if (!var_id.has_value()) {
      return false;
    }
    if (!IsVariableDefined(var_id.value())) {
      return false;
    }
  }

  return true;
}

// =============================================================================
// Instruction-Specific Verifiers
// =============================================================================

VerificationResult Verifier::VerifyReturn(const Instruction& inst,
                                           size_t index) {
  // Return should have 0 or 1 operands
  if (inst.operands.size() > 1) {
    return VerificationResult::Failure(
        "Return instruction has more than 1 operand", index, inst.kind);
  }

  // If there's a return value, it must be a valid value (not variable)
  if (!inst.operands.empty()) {
    const Value& ret_val = inst.operands[0];
    if (!ret_val.IsValue()) {
      return VerificationResult::Failure(
          "Return value must be a value (constant or SSA), not a variable reference",
          index, inst.kind);
    }
    if (!IsValidValue(ret_val, "return")) {
      return VerificationResult::Failure(
          "Return value is invalid or references undefined SSA/variable",
          index, inst.kind);
    }
  }

  // Return should not produce a result
  if (inst.result_id != 0) {
    return VerificationResult::Failure(
        "Return instruction should not have a result_id", index, inst.kind);
  }

  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyVariable(const Instruction& inst,
                                             size_t index) {
  // Variable declaration should have no operands (initializer is a separate store)
  if (!inst.operands.empty()) {
    return VerificationResult::Failure(
        "Variable declaration should have no operands (use separate store for initializer)",
        index, inst.kind);
  }

  // Must have a valid variable id
  if (inst.var_id == 0) {
    return VerificationResult::Failure(
        "Variable declaration has no var_id", index, inst.kind);
  }

  // Must have a valid type
  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Variable declaration has invalid type", index, inst.kind);
  }

  // Variable should not have a result_id (it has var_id instead)
  if (inst.result_id != 0) {
    return VerificationResult::Failure(
        "Variable declaration should use var_id, not result_id", index, inst.kind);
  }

  // Track this variable definition
  TrackVariableDefinition(inst.var_id);

  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyLoad(const Instruction& inst,
                                         size_t index) {
  // Load must have exactly 1 operand (source variable)
  if (inst.operands.size() != 1) {
    return VerificationResult::Failure(
        "Load instruction must have exactly 1 operand", index, inst.kind);
  }

  // Operand must be a variable reference
  const Value& source = inst.operands[0];
  if (!source.IsVariable()) {
    return VerificationResult::Failure(
        "Load source must be a variable reference", index, inst.kind);
  }

  if (!IsValidValue(source, "load source")) {
    return VerificationResult::Failure(
        "Load source references undefined variable", index, inst.kind);
  }

  // Load must produce a valid SSA result
  if (inst.result_id == 0) {
    return VerificationResult::Failure(
        "Load instruction must produce a result_id", index, inst.kind);
  }

  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Load instruction must have a valid result type", index, inst.kind);
  }

  // Track the SSA definition
  TrackSSADefinition(inst.result_id);

  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyStore(const Instruction& inst,
                                          size_t index) {
  // Store must have exactly 2 operands: target (variable), source (value)
  if (inst.operands.size() != 2) {
    return VerificationResult::Failure(
        "Store instruction must have exactly 2 operands (target variable, source value)",
        index, inst.kind);
  }

  // Target must be a variable reference
  const Value& target = inst.operands[0];
  if (!target.IsVariable()) {
    return VerificationResult::Failure(
        "Store target must be a variable reference", index, inst.kind);
  }

  if (!IsValidValue(target, "store target")) {
    return VerificationResult::Failure(
        "Store target references undefined variable", index, inst.kind);
  }

  // Source must be a value (constant or SSA), not a variable reference
  const Value& source = inst.operands[1];
  if (!source.IsValue()) {
    return VerificationResult::Failure(
        "Store source must be a value (constant or SSA), not a variable reference",
        index, inst.kind);
  }

  if (!IsValidValue(source, "store source")) {
    return VerificationResult::Failure(
        "Store source is invalid or references undefined SSA", index, inst.kind);
  }

  // Store should not produce a result
  if (inst.result_id != 0) {
    return VerificationResult::Failure(
        "Store instruction should not have a result_id", index, inst.kind);
  }

  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyBinary(const Instruction& inst,
                                           size_t index) {
  // Binary must have exactly 2 operands (lhs, rhs)
  if (inst.operands.size() != 2) {
    return VerificationResult::Failure(
        "Binary instruction must have exactly 2 operands", index, inst.kind);
  }

  // Both operands must be values
  const Value& lhs = inst.operands[0];
  const Value& rhs = inst.operands[1];

  if (!lhs.IsValue()) {
    return VerificationResult::Failure(
        "Binary lhs must be a value (constant or SSA)", index, inst.kind);
  }

  if (!rhs.IsValue()) {
    return VerificationResult::Failure(
        "Binary rhs must be a value (constant or SSA)", index, inst.kind);
  }

  if (!IsValidValue(lhs, "binary lhs")) {
    return VerificationResult::Failure(
        "Binary lhs is invalid or references undefined SSA", index, inst.kind);
  }

  if (!IsValidValue(rhs, "binary rhs")) {
    return VerificationResult::Failure(
        "Binary rhs is invalid or references undefined SSA", index, inst.kind);
  }

  // Binary must produce a valid SSA result
  if (inst.result_id == 0) {
    return VerificationResult::Failure(
        "Binary instruction must produce a result_id", index, inst.kind);
  }

  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Binary instruction must have a valid result type", index, inst.kind);
  }

  // Track the SSA definition
  TrackSSADefinition(inst.result_id);

  return VerificationResult::Success();
}

// =============================================================================
// Convenience Functions
// =============================================================================

VerificationResult Verify(const Module& module) {
  Verifier verifier;
  return verifier.VerifyModule(module);
}

VerificationResult Verify(const Function& function) {
  Verifier verifier;
  return verifier.VerifyFunction(function);
}

}  // namespace ir
}  // namespace wgx
