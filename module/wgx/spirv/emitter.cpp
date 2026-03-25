// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter.h"

#include <cstdint>
#include <string_view>
#include <vector>

#include "spirv/unified1/spirv.h"

namespace wgx {
namespace spirv {

namespace {

constexpr uint32_t kSpirvVersion13 = 0x00010300u;

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
  if (words == nullptr) {
    return;
  }

  const auto word_count = static_cast<uint32_t>(1u + operands.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(opcode));
  words->insert(words->end(), operands.begin(), operands.end());
}

void AppendEntryPoint(std::vector<uint32_t>* words, SpvExecutionModel model,
                      uint32_t function_id, std::string_view entry_point) {
  if (words == nullptr) {
    return;
  }

  auto name_words = EncodeStringLiteral(entry_point);
  const auto word_count = static_cast<uint32_t>(3u + name_words.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(SpvOpEntryPoint));
  words->push_back(static_cast<uint32_t>(model));
  words->push_back(function_id);
  words->insert(words->end(), name_words.begin(), name_words.end());
}

void AppendName(std::vector<uint32_t>* words, uint32_t target_id,
                std::string_view name) {
  if (words == nullptr) {
    return;
  }

  auto name_words = EncodeStringLiteral(name);
  const auto word_count = static_cast<uint32_t>(2u + name_words.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(SpvOpName));
  words->push_back(target_id);
  words->insert(words->end(), name_words.begin(), name_words.end());
}

void AppendModuleProcessed(std::vector<uint32_t>* words,
                           std::string_view process) {
  if (words == nullptr) {
    return;
  }

  auto process_words = EncodeStringLiteral(process);
  const auto word_count = static_cast<uint32_t>(1u + process_words.size());
  words->push_back((word_count << SpvWordCountShift) |
                   static_cast<uint32_t>(SpvOpModuleProcessed));
  words->insert(words->end(), process_words.begin(), process_words.end());
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

bool SupportsCurrentIR(const ir::Function& function) {
  if (function.entry_block.instructions.size() != 1u) {
    return false;
  }

  const auto& inst = function.entry_block.instructions.front();
  return inst.kind == ir::InstKind::kReturn && !inst.has_return_value;
}

std::string PackBinaryModule(const std::vector<uint32_t>& words) {
  std::string result(words.size() * sizeof(uint32_t), '\0');

  for (size_t i = 0; i < words.size(); ++i) {
    const auto word = words[i];
    result[i * 4u + 0u] = static_cast<char>(word & 0xffu);
    result[i * 4u + 1u] = static_cast<char>((word >> 8u) & 0xffu);
    result[i * 4u + 2u] = static_cast<char>((word >> 16u) & 0xffu);
    result[i * 4u + 3u] = static_cast<char>((word >> 24u) & 0xffu);
  }

  return result;
}

}  // namespace

bool Emitter::Emit(const ir::Module& module) {
  result_.clear();

  if (module.entry_point.empty() ||
      module.stage == ir::PipelineStage::kUnknown) {
    return false;
  }

  if (module.functions.empty()) {
    return false;
  }

  const ir::Function* entry_function = nullptr;
  for (const auto& function : module.functions) {
    if (function.name == module.entry_point) {
      entry_function = &function;
      break;
    }
  }

  if (entry_function == nullptr || entry_function->stage != module.stage) {
    return false;
  }

  if (!SupportsCurrentIR(*entry_function)) {
    return false;
  }

  const auto execution_model = ToExecutionModel(module.stage);
  if (execution_model == SpvExecutionModelMax) {
    return false;
  }

  constexpr uint32_t kVoidTypeId = 1u;
  constexpr uint32_t kFunctionTypeId = 2u;
  constexpr uint32_t kFunctionId = 3u;
  constexpr uint32_t kLabelId = 4u;
  constexpr uint32_t kIdBound = 5u;

  std::vector<uint32_t> words;
  words.reserve(32u);

  words.push_back(SpvMagicNumber);
  words.push_back(kSpirvVersion13);
  words.push_back(0u);
  words.push_back(kIdBound);
  words.push_back(0u);

  AppendInstruction(&words, SpvOpCapability,
                    {static_cast<uint32_t>(SpvCapabilityShader)});
  AppendInstruction(&words, SpvOpMemoryModel,
                    {static_cast<uint32_t>(SpvAddressingModelLogical),
                     static_cast<uint32_t>(SpvMemoryModelGLSL450)});
  AppendEntryPoint(&words, execution_model, kFunctionId, module.entry_point);

  if (module.stage == ir::PipelineStage::kFragment) {
    AppendInstruction(
        &words, SpvOpExecutionMode,
        {kFunctionId, static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
  }

  AppendName(&words, kFunctionId, module.entry_point);
  AppendModuleProcessed(&words, "skity-wgx-dev");
  AppendInstruction(&words, SpvOpTypeVoid, {kVoidTypeId});
  AppendInstruction(&words, SpvOpTypeFunction, {kFunctionTypeId, kVoidTypeId});
  AppendInstruction(
      &words, SpvOpFunction,
      {kVoidTypeId, kFunctionId,
       static_cast<uint32_t>(SpvFunctionControlMaskNone), kFunctionTypeId});
  AppendInstruction(&words, SpvOpLabel, {kLabelId});
  AppendInstruction(&words, SpvOpReturn, {});
  AppendInstruction(&words, SpvOpFunctionEnd, {});

  result_ = PackBinaryModule(words);
  return true;
}

}  // namespace spirv
}  // namespace wgx
