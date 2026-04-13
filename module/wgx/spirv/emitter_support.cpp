// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <string_view>

#include "ir/verifier.h"
#include "spirv/emitter.h"
#include "spirv/emitter_internal.h"

namespace wgx {
namespace spirv {

std::vector<uint32_t> EncodeStringLiteral(std::string_view value) {
  const size_t word_count = (value.size() + 1u + 3u) / 4u;
  std::vector<uint32_t> words(word_count, 0u);
  for (size_t i = 0; i < value.size(); ++i) {
    words[i / 4u] |= static_cast<uint32_t>(static_cast<uint8_t>(value[i]))
                     << ((i % 4u) * 8u);
  }
  return words;
}

void AppendInstruction(std::vector<uint32_t>* words, SpvOp opcode,
                       std::initializer_list<uint32_t> operands) {
  if (words == nullptr) return;
  const auto word_count = static_cast<uint32_t>(1u + operands.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(opcode));
  words->insert(words->end(), operands.begin(), operands.end());
}

void AppendInstruction(std::vector<uint32_t>* words, SpvOp opcode,
                       const std::vector<uint32_t>& operands) {
  if (words == nullptr) return;
  const auto word_count = static_cast<uint32_t>(1u + operands.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(opcode));
  words->insert(words->end(), operands.begin(), operands.end());
}

void AppendEntryPoint(std::vector<uint32_t>* words, SpvExecutionModel model,
                      uint32_t function_id, std::string_view entry_point,
                      const std::vector<uint32_t>& interfaces) {
  if (words == nullptr) return;
  auto name_words = EncodeStringLiteral(entry_point);
  const auto word_count =
      static_cast<uint32_t>(3u + name_words.size() + interfaces.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(SpvOpEntryPoint));
  words->push_back(static_cast<uint32_t>(model));
  words->push_back(function_id);
  words->insert(words->end(), name_words.begin(), name_words.end());
  words->insert(words->end(), interfaces.begin(), interfaces.end());
}

void AppendName(std::vector<uint32_t>* words, uint32_t target_id,
                std::string_view name) {
  if (words == nullptr) return;
  auto name_words = EncodeStringLiteral(name);
  const auto word_count = static_cast<uint32_t>(2u + name_words.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(SpvOpName));
  words->push_back(target_id);
  words->insert(words->end(), name_words.begin(), name_words.end());
}

void AppendSection(std::vector<uint32_t>* dst,
                   const std::vector<uint32_t>& src) {
  if (dst == nullptr || src.empty()) return;
  dst->insert(dst->end(), src.begin(), src.end());
}

SpvExecutionModel ToExecutionModel(ir::PipelineStage stage) {
  switch (stage) {
    case ir::PipelineStage::kVertex:
      return SpvExecutionModelVertex;
    case ir::PipelineStage::kFragment:
      return SpvExecutionModelFragment;
    case ir::PipelineStage::kUnknown:
      break;
  }
  return SpvExecutionModelMax;
}

SpvStorageClass ToSpvStorageClass(ir::StorageClass storage) {
  switch (storage) {
    case ir::StorageClass::kFunction:
      return SpvStorageClassFunction;
    case ir::StorageClass::kPrivate:
      return SpvStorageClassPrivate;
    case ir::StorageClass::kUniform:
      return SpvStorageClassUniform;
    case ir::StorageClass::kStorage:
      return SpvStorageClassStorageBuffer;
    case ir::StorageClass::kOutput:
      return SpvStorageClassOutput;
    case ir::StorageClass::kInput:
      return SpvStorageClassInput;
    case ir::StorageClass::kWorkgroup:
      return SpvStorageClassWorkgroup;
    case ir::StorageClass::kHandle:
      return SpvStorageClassUniformConstant;
  }
  return SpvStorageClassMax;
}

bool SupportsCurrentIR(const ir::Function& function) {
  auto result = ir::Verify(function);
  if (!result.valid) {
    return false;
  }

  if (!function.output_vars.empty()) {
    if (function.output_vars.size() != 1) {
      return false;
    }

    const auto& output = function.output_vars[0];

    if (function.stage == ir::PipelineStage::kVertex) {
      if (output.decoration_kind != ir::OutputDecorationKind::kBuiltin ||
          output.GetBuiltin() != ir::BuiltinType::kPosition) {
        return false;
      }
    } else if (function.stage == ir::PipelineStage::kFragment) {
      if (output.decoration_kind != ir::OutputDecorationKind::kLocation) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

uint32_t FloatToBits(float value) {
  uint32_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(uint32_t));
  return bits;
}

const ir::Function* FindEntryFunction(const ir::Module& module) {
  for (const auto& function : module.functions) {
    if (function.name == module.entry_point) {
      return &function;
    }
  }
  return nullptr;
}

bool Emitter::Emit(const ir::Module& module) {
  result_.clear();

  if (module.entry_point.empty() ||
      module.stage == ir::PipelineStage::kUnknown) {
    return false;
  }
  if (module.functions.empty()) return false;
  if (!ir::Verify(module).valid) return false;

  const ir::Function* entry_function = FindEntryFunction(module);
  if (entry_function == nullptr || entry_function->stage != module.stage) {
    return false;
  }
  if (!SupportsCurrentIR(*entry_function)) return false;

  const auto execution_model = ToExecutionModel(module.stage);
  if (execution_model == SpvExecutionModelMax) return false;

  SectionBuffers sections;
  std::vector<uint32_t> words;
  ModuleBuilder builder(module, *entry_function, execution_model);
  if (!builder.Build(&sections, &words)) return false;

  result_ = std::move(words);
  return true;
}

}  // namespace spirv
}  // namespace wgx
