// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ir/module.h"

namespace wgx {
namespace ir {

/**
 * VerificationResult - Result of IR verification
 *
 * On success: valid == true
 * On failure: valid == false, with error message and location info
 */
struct VerificationResult {
  bool valid = true;
  std::string error_message;

  // Location info for debugging
  size_t instruction_index = 0;
  InstKind instruction_kind = InstKind::kReturn;

  static VerificationResult Success() { return VerificationResult{}; }

  static VerificationResult Failure(const std::string& msg,
                                    size_t inst_index = 0,
                                    InstKind inst_kind = InstKind::kReturn) {
    VerificationResult result;
    result.valid = false;
    result.error_message = msg;
    result.instruction_index = inst_index;
    result.instruction_kind = inst_kind;
    return result;
  }
};

/**
 * IR Verifier - Validates structural correctness of IR
 *
 * The verifier checks:
 * - Instruction shape validity (operand counts, result types)
 * - Basic type consistency
 * - Use-def legality (SSA values are defined before use)
 * - Block terminator rules
 *
 * Note: This is a structural verifier, not a semantic verifier.
 * Backend-specific capability checks should be done separately.
 */
class Verifier {
 public:
  Verifier() = default;
  ~Verifier() = default;

  // Disallow copy and move
  Verifier(const Verifier&) = delete;
  Verifier& operator=(const Verifier&) = delete;

  /**
   * Verify a complete module
   */
  VerificationResult VerifyModule(const Module& module);

  /**
   * Verify a single function
   */
  VerificationResult VerifyFunction(const Function& function);

  /**
   * Verify a single instruction (within function context)
   */
  VerificationResult VerifyInstruction(const Instruction& inst, size_t index,
                                       const Function& function);

 private:
  // Per-function verification state
  std::vector<uint32_t> defined_ssa_ids_;
  std::vector<uint32_t> defined_var_ids_;
  std::vector<uint32_t>
      referenced_var_ids_;  // For global variables/params without kVariable

  void ResetState();

  // Track definitions
  void TrackSSADefinition(uint32_t ssa_id);
  void TrackVariableDefinition(uint32_t var_id);
  void TrackVariableReference(uint32_t var_id);

  // Check if value is valid (type is valid, SSA ids are defined, etc.)
  // Non-const because it may track new variable references
  bool IsValidValue(const Value& value, const std::string& context);
  bool IsSSADefined(uint32_t ssa_id) const;
  bool IsVariableDefined(uint32_t var_id) const;
  bool IsVariableTracked(uint32_t var_id) const;

  // Specific instruction verifiers
  VerificationResult VerifyReturn(const Instruction& inst, size_t index);
  VerificationResult VerifyVariable(const Instruction& inst, size_t index);
  VerificationResult VerifyLoad(const Instruction& inst, size_t index);
  VerificationResult VerifyStore(const Instruction& inst, size_t index);
  VerificationResult VerifyBinary(const Instruction& inst, size_t index);
};

/**
 * Convenience function for one-shot verification
 */
VerificationResult Verify(const Module& module);
VerificationResult Verify(const Function& function);

}  // namespace ir
}  // namespace wgx
