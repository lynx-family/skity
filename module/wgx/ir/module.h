// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <string>
#include <vector>

namespace wgx {
namespace ir {

enum class PipelineStage {
  kUnknown,
  kVertex,
  kFragment,
};

enum class InstKind {
  kReturn,
};

struct Instruction {
  InstKind kind = InstKind::kReturn;
  bool has_return_value = false;
};

struct Block {
  std::vector<Instruction> instructions = {};
};

struct Function {
  std::string name = {};
  PipelineStage stage = PipelineStage::kUnknown;
  Block entry_block = {};
};

struct Module {
  std::string entry_point = {};
  PipelineStage stage = PipelineStage::kUnknown;
  std::vector<Function> functions = {};
};

}  // namespace ir
}  // namespace wgx
