// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter.h"
#include "spirv/unified1/spirv.h"

namespace wgx {
namespace spirv {

bool Emitter::Emit(const ir::Module& module) {
  if (module.entry_point.empty() || module.stage == ir::PipelineStage::kUnknown) {
    return false;
  }

  if (module.functions.empty()) {
    return false;
  }

  const auto& entry_function = module.functions.front();
  bool has_return = false;
  for (const auto& inst : entry_function.entry_block.instructions) {
    if (inst.kind == ir::InstKind::kReturn) {
      has_return = true;
      break;
    }
  }

  if (!has_return) {
    return false;
  }

  // Placeholder text format for initial Vulkan path integration.
  result_ = "; SPIR-V emission is under development\n"
            "; entry_point=" +
            module.entry_point + "\n"
            "; inst_count=" +
            std::to_string(entry_function.entry_block.instructions.size()) +
            "\n";
  return true;
}

}  // namespace spirv
}  // namespace wgx
