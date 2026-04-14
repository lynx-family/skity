// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ir/verifier.h"

#include <algorithm>

namespace wgx {
namespace ir {

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

  if (function.name.empty()) {
    return VerificationResult::Failure("Function has no name");
  }
  if (function.entry_block_id == kInvalidBlockId) {
    return VerificationResult::Failure("Function has no entry block");
  }
  if (function.blocks.empty()) {
    return VerificationResult::Failure("Function has no blocks");
  }
  if (function.GetBlock(function.entry_block_id) == nullptr) {
    return VerificationResult::Failure("Entry block id does not exist");
  }

  for (size_t block_index = 0; block_index < function.blocks.size();
       ++block_index) {
    const Block& block = function.blocks[block_index];
    if (block.id == kInvalidBlockId) {
      return VerificationResult::Failure("Block has invalid id", 0,
                                         InstKind::kReturn, block_index);
    }
    if (block.instructions.empty()) {
      return VerificationResult::Failure("Block has no instructions", 0,
                                         InstKind::kReturn, block_index);
    }
    if ((block.loop_merge_block == kInvalidBlockId) !=
        (block.loop_continue_block == kInvalidBlockId)) {
      return VerificationResult::Failure(
          "Loop header block must provide both merge and continue targets", 0,
          InstKind::kBranch, block_index);
    }
    if (block.IsLoopHeader()) {
      if (function.GetBlock(block.loop_merge_block) == nullptr ||
          function.GetBlock(block.loop_continue_block) == nullptr) {
        return VerificationResult::Failure(
            "Loop header references a non-existent merge or continue block", 0,
            InstKind::kBranch, block_index);
      }
    }

    for (size_t i = 0; i < block.instructions.size(); ++i) {
      auto result =
          VerifyInstruction(block.instructions[i], i, block_index, function);
      if (!result.valid) {
        return result;
      }
      if (block.instructions[i].IsTerminator() &&
          i + 1 < block.instructions.size()) {
        return VerificationResult::Failure(
            "Terminator instruction must be the last instruction in a block", i,
            block.instructions[i].kind, block_index);
      }
    }

    const auto& last_inst = block.instructions.back();
    if (!last_inst.IsTerminator()) {
      return VerificationResult::Failure(
          "Block does not end with a terminator instruction",
          block.instructions.size() - 1, last_inst.kind, block_index);
    }
  }

  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyInstruction(const Instruction& inst,
                                               size_t index, size_t block_index,
                                               const Function& function) {
  switch (inst.kind) {
    case InstKind::kReturn:
      return VerifyReturn(inst, index);
    case InstKind::kVariable:
      return VerifyVariable(inst, index);
    case InstKind::kLoad:
      return VerifyLoad(inst, index);
    case InstKind::kStore:
      return VerifyStore(inst, index);
    case InstKind::kAccess:
      return VerifyAccess(inst, index);
    case InstKind::kExtract:
      return VerifyExtract(inst, index);
    case InstKind::kBinary:
      return VerifyBinary(inst, index);
    case InstKind::kConstruct:
      return VerifyConstruct(inst, index);
    case InstKind::kCall:
      return VerifyCall(inst, index);
    case InstKind::kBuiltinCall:
      return VerifyBuiltinCall(inst, index);
    case InstKind::kBranch:
      return VerifyBranch(inst, index, function);
    case InstKind::kCondBranch:
      return VerifyCondBranch(inst, index, function);
    default:
      return VerificationResult::Failure("Unknown instruction kind", index,
                                         inst.kind, block_index);
  }
}

void Verifier::ResetState() {
  defined_ssa_ids_.clear();
  defined_var_ids_.clear();
  referenced_var_ids_.clear();
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
  return std::find(defined_ssa_ids_.begin(), defined_ssa_ids_.end(), ssa_id) !=
         defined_ssa_ids_.end();
}

bool Verifier::IsVariableDefined(uint32_t var_id) const {
  if (var_id == 0) return false;
  return std::find(defined_var_ids_.begin(), defined_var_ids_.end(), var_id) !=
         defined_var_ids_.end();
}

bool Verifier::IsVariableTracked(uint32_t var_id) const {
  if (IsVariableDefined(var_id)) {
    return true;
  }
  if (std::find(referenced_var_ids_.begin(), referenced_var_ids_.end(),
                var_id) != referenced_var_ids_.end()) {
    return true;
  }
  return false;
}

void Verifier::TrackVariableReference(uint32_t var_id) {
  if (var_id != 0 && !IsVariableTracked(var_id)) {
    referenced_var_ids_.push_back(var_id);
  }
}

bool Verifier::IsValidValue(const Value& value, const std::string& context) {
  (void)context;
  if (!value.IsValid()) {
    return false;
  }

  if (value.IsSSA()) {
    auto ssa_id = value.GetSSAId();
    if (!ssa_id.has_value()) {
      return false;
    }
    if (!IsSSADefined(ssa_id.value())) {
      return false;
    }
  }

  if (value.IsPointerSSA()) {
    auto ssa_id = value.GetSSAId();
    if (!ssa_id.has_value()) {
      return false;
    }
    if (!IsSSADefined(ssa_id.value())) {
      return false;
    }
  }

  if (value.IsVariable()) {
    auto var_id = value.GetVarId();
    if (!var_id.has_value()) {
      return false;
    }
    uint32_t id = var_id.value();
    if (!IsVariableTracked(id)) {
      TrackVariableReference(id);
    }
  }

  return true;
}

VerificationResult Verifier::VerifyReturn(const Instruction& inst,
                                          size_t index) {
  for (const auto& ret_val : inst.operands) {
    if (!ret_val.IsValue()) {
      return VerificationResult::Failure(
          "Return value must be a value (constant or SSA), not a variable "
          "reference",
          index, inst.kind);
    }
    if (!IsValidValue(ret_val, "return")) {
      return VerificationResult::Failure(
          "Return value is invalid or references undefined SSA/variable", index,
          inst.kind);
    }
  }
  if (inst.result_id != 0) {
    return VerificationResult::Failure(
        "Return instruction should not have a result_id", index, inst.kind);
  }
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyVariable(const Instruction& inst,
                                            size_t index) {
  if (!inst.operands.empty()) {
    return VerificationResult::Failure(
        "Variable declaration should have no operands (use separate store for "
        "initializer)",
        index, inst.kind);
  }
  if (inst.var_id == 0) {
    return VerificationResult::Failure("Variable declaration has no var_id",
                                       index, inst.kind);
  }
  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure("Variable declaration has invalid type",
                                       index, inst.kind);
  }
  if (inst.result_id != 0) {
    return VerificationResult::Failure(
        "Variable declaration should use var_id, not result_id", index,
        inst.kind);
  }
  TrackVariableDefinition(inst.var_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyLoad(const Instruction& inst, size_t index) {
  if (inst.operands.size() != 1) {
    return VerificationResult::Failure(
        "Load instruction must have exactly 1 operand", index, inst.kind);
  }
  const Value& source = inst.operands[0];
  if (!source.IsAddress()) {
    return VerificationResult::Failure("Load source must be an address", index,
                                       inst.kind);
  }
  if (!IsValidValue(source, "load source")) {
    return VerificationResult::Failure("Load source is invalid or undefined",
                                       index, inst.kind);
  }
  if (inst.result_id == 0) {
    return VerificationResult::Failure(
        "Load instruction must produce a result_id", index, inst.kind);
  }
  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Load instruction must have a valid result type", index, inst.kind);
  }
  TrackSSADefinition(inst.result_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyStore(const Instruction& inst,
                                         size_t index) {
  if (inst.operands.size() != 2) {
    return VerificationResult::Failure(
        "Store instruction must have exactly 2 operands (target variable, "
        "source value)",
        index, inst.kind);
  }
  const Value& target = inst.operands[0];
  const Value& source = inst.operands[1];
  if (!target.IsAddress()) {
    return VerificationResult::Failure("Store target must be an address", index,
                                       inst.kind);
  }
  if (!IsValidValue(target, "store target")) {
    return VerificationResult::Failure("Store target is invalid or undefined",
                                       index, inst.kind);
  }
  if (!source.IsValue()) {
    return VerificationResult::Failure(
        "Store source must be a value (constant or SSA)", index, inst.kind);
  }
  if (!IsValidValue(source, "store source")) {
    return VerificationResult::Failure("Store source is invalid or undefined",
                                       index, inst.kind);
  }
  if (inst.result_id != 0) {
    return VerificationResult::Failure(
        "Store instruction should not have a result_id", index, inst.kind);
  }
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyAccess(const Instruction& inst,
                                          size_t index) {
  if (inst.operands.size() != 1) {
    return VerificationResult::Failure(
        "Access instruction must have exactly 1 operand", index, inst.kind);
  }
  const Value& base = inst.operands[0];
  if (!base.IsAddress()) {
    return VerificationResult::Failure("Access base must be an address", index,
                                       inst.kind);
  }
  if (!IsValidValue(base, "access base")) {
    return VerificationResult::Failure("Access base is invalid or undefined",
                                       index, inst.kind);
  }
  if (inst.result_id == 0) {
    return VerificationResult::Failure(
        "Access instruction must produce a result_id", index, inst.kind);
  }
  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Access instruction must have a valid result type", index, inst.kind);
  }
  TrackSSADefinition(inst.result_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyExtract(const Instruction& inst,
                                           size_t index) {
  if (inst.operands.size() != 1) {
    return VerificationResult::Failure(
        "Extract instruction must have exactly 1 operand", index, inst.kind);
  }
  const Value& base = inst.operands[0];
  if (!base.IsValue()) {
    return VerificationResult::Failure("Extract base must be a value", index,
                                       inst.kind);
  }
  if (!IsValidValue(base, "extract base")) {
    return VerificationResult::Failure("Extract base is invalid or undefined",
                                       index, inst.kind);
  }
  if (inst.result_id == 0 || inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Extract instruction must produce a typed result", index, inst.kind);
  }
  TrackSSADefinition(inst.result_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyBinary(const Instruction& inst,
                                          size_t index) {
  if (inst.operands.size() != 2) {
    return VerificationResult::Failure(
        "Binary instruction must have exactly 2 operands", index, inst.kind);
  }
  const Value& lhs = inst.operands[0];
  const Value& rhs = inst.operands[1];
  if (!lhs.IsValue()) {
    return VerificationResult::Failure("Binary lhs must be a value", index,
                                       inst.kind);
  }
  if (!rhs.IsValue()) {
    return VerificationResult::Failure("Binary rhs must be a value", index,
                                       inst.kind);
  }
  if (!IsValidValue(lhs, "binary lhs")) {
    return VerificationResult::Failure("Binary lhs is invalid or undefined",
                                       index, inst.kind);
  }
  if (!IsValidValue(rhs, "binary rhs")) {
    return VerificationResult::Failure("Binary rhs is invalid or undefined",
                                       index, inst.kind);
  }
  if (inst.result_id == 0) {
    return VerificationResult::Failure(
        "Binary instruction must produce a result_id", index, inst.kind);
  }
  if (inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Binary instruction must have a valid result type", index, inst.kind);
  }
  TrackSSADefinition(inst.result_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyConstruct(const Instruction& inst,
                                             size_t index) {
  if (inst.operands.empty()) {
    return VerificationResult::Failure(
        "Construct instruction must have at least 1 operand", index, inst.kind);
  }
  if (inst.result_id == 0 || inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Construct instruction must produce a typed result", index, inst.kind);
  }
  for (const auto& operand : inst.operands) {
    if (!operand.IsValue()) {
      return VerificationResult::Failure("Construct operands must be values",
                                         index, inst.kind);
    }
    if (!IsValidValue(operand, "construct operand")) {
      return VerificationResult::Failure(
          "Construct operand is invalid or undefined", index, inst.kind);
    }
  }
  TrackSSADefinition(inst.result_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyCall(const Instruction& inst, size_t index) {
  if (inst.callee_name.empty()) {
    return VerificationResult::Failure("Call instruction has no callee name",
                                       index, inst.kind);
  }

  for (const auto& operand : inst.operands) {
    if (!operand.IsValue()) {
      return VerificationResult::Failure("Call operands must be values", index,
                                         inst.kind);
    }
    if (!IsValidValue(operand, "call operand")) {
      return VerificationResult::Failure("Call operand is invalid or undefined",
                                         index, inst.kind);
    }
  }

  const bool has_result =
      inst.result_id != 0 || inst.result_type != kInvalidTypeId;
  if (has_result) {
    if (inst.result_id == 0 || inst.result_type == kInvalidTypeId) {
      return VerificationResult::Failure(
          "Call instruction result must provide both result_id and result_type",
          index, inst.kind);
    }
    TrackSSADefinition(inst.result_id);
  }
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyBuiltinCall(const Instruction& inst,
                                               size_t index) {
  if (inst.builtin_call == BuiltinCallKind::kNone) {
    return VerificationResult::Failure(
        "Builtin call instruction has no builtin kind", index, inst.kind);
  }
  if (inst.result_id == 0 || inst.result_type == kInvalidTypeId) {
    return VerificationResult::Failure(
        "Builtin call instruction must produce a typed result", index,
        inst.kind);
  }

  for (const auto& operand : inst.operands) {
    if (!operand.IsValue()) {
      return VerificationResult::Failure("Builtin call operands must be values",
                                         index, inst.kind);
    }
    if (!IsValidValue(operand, "builtin call operand")) {
      return VerificationResult::Failure(
          "Builtin call operand is invalid or undefined", index, inst.kind);
    }
  }

  TrackSSADefinition(inst.result_id);
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyBranch(const Instruction& inst, size_t index,
                                          const Function& function) {
  if (!inst.operands.empty()) {
    return VerificationResult::Failure(
        "Branch instruction should not have operands", index, inst.kind);
  }
  if (inst.target_block == kInvalidBlockId ||
      function.GetBlock(inst.target_block) == nullptr) {
    return VerificationResult::Failure("Branch target block does not exist",
                                       index, inst.kind);
  }
  return VerificationResult::Success();
}

VerificationResult Verifier::VerifyCondBranch(const Instruction& inst,
                                              size_t index,
                                              const Function& function) {
  if (inst.operands.size() != 1) {
    return VerificationResult::Failure(
        "CondBranch must have exactly 1 condition operand", index, inst.kind);
  }
  const Value& condition = inst.operands[0];
  if (!condition.IsValue()) {
    return VerificationResult::Failure("CondBranch condition must be a value",
                                       index, inst.kind);
  }
  if (!IsValidValue(condition, "cond branch condition")) {
    return VerificationResult::Failure(
        "CondBranch condition is invalid or undefined", index, inst.kind);
  }
  if (condition.type == kInvalidTypeId) {
    return VerificationResult::Failure("CondBranch condition has invalid type",
                                       index, inst.kind);
  }
  if (function.GetBlock(inst.true_block) == nullptr ||
      function.GetBlock(inst.false_block) == nullptr) {
    return VerificationResult::Failure("CondBranch target block does not exist",
                                       index, inst.kind);
  }
  if (inst.merge_block != kInvalidBlockId &&
      function.GetBlock(inst.merge_block) == nullptr) {
    return VerificationResult::Failure("CondBranch merge block does not exist",
                                       index, inst.kind);
  }
  return VerificationResult::Success();
}

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
