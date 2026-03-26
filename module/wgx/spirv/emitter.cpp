// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter.h"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <unordered_map>
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
                      uint32_t function_id, std::string_view entry_point,
                      std::initializer_list<uint32_t> interfaces = {}) {
  if (words == nullptr) {
    return;
  }

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
  if (inst.kind != ir::InstKind::kReturn) {
    return false;
  }

  if (!inst.has_return_value) {
    return true;
  }

  return function.stage == ir::PipelineStage::kVertex &&
         function.return_builtin_position &&
         inst.return_value_kind == ir::ReturnValueKind::kConstVec4F32;
}

uint32_t FloatToBits(float value) {
  uint32_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(uint32_t));
  return bits;
}

class IdAllocator {
 public:
  uint32_t Allocate() { return next_id_++; }

  uint32_t Bound() const { return next_id_; }

 private:
  uint32_t next_id_ = 1u;
};

struct TypeCache {
  uint32_t void_type = 0u;
  uint32_t function_void = 0u;
  uint32_t f32 = 0u;
  uint32_t vec4_f32 = 0u;
  uint32_t ptr_output_vec4_f32 = 0u;
};

uint32_t GetOrCreateVoidType(IdAllocator* ids, TypeCache* types,
                             std::vector<uint32_t>* words) {
  if (ids == nullptr || types == nullptr || words == nullptr) {
    return 0u;
  }

  if (types->void_type != 0u) {
    return types->void_type;
  }

  types->void_type = ids->Allocate();
  AppendInstruction(words, SpvOpTypeVoid, {types->void_type});
  return types->void_type;
}

uint32_t GetOrCreateFunctionTypeVoid(IdAllocator* ids, TypeCache* types,
                                     std::vector<uint32_t>* words) {
  if (ids == nullptr || types == nullptr || words == nullptr) {
    return 0u;
  }

  if (types->function_void != 0u) {
    return types->function_void;
  }

  const auto void_type = GetOrCreateVoidType(ids, types, words);
  if (void_type == 0u) {
    return 0u;
  }

  types->function_void = ids->Allocate();
  AppendInstruction(words, SpvOpTypeFunction, {types->function_void, void_type});
  return types->function_void;
}

uint32_t GetOrCreateF32Type(IdAllocator* ids, TypeCache* types,
                            std::vector<uint32_t>* words) {
  if (ids == nullptr || types == nullptr || words == nullptr) {
    return 0u;
  }

  if (types->f32 != 0u) {
    return types->f32;
  }

  types->f32 = ids->Allocate();
  AppendInstruction(words, SpvOpTypeFloat, {types->f32, 32u});
  return types->f32;
}

uint32_t GetOrCreateVec4F32Type(IdAllocator* ids, TypeCache* types,
                                std::vector<uint32_t>* words) {
  if (ids == nullptr || types == nullptr || words == nullptr) {
    return 0u;
  }

  if (types->vec4_f32 != 0u) {
    return types->vec4_f32;
  }

  const auto f32_type = GetOrCreateF32Type(ids, types, words);
  if (f32_type == 0u) {
    return 0u;
  }

  types->vec4_f32 = ids->Allocate();
  AppendInstruction(words, SpvOpTypeVector, {types->vec4_f32, f32_type, 4u});
  return types->vec4_f32;
}

uint32_t GetOrCreatePtrOutputVec4F32Type(IdAllocator* ids, TypeCache* types,
                                         std::vector<uint32_t>* words) {
  if (ids == nullptr || types == nullptr || words == nullptr) {
    return 0u;
  }

  if (types->ptr_output_vec4_f32 != 0u) {
    return types->ptr_output_vec4_f32;
  }

  const auto vec4_type = GetOrCreateVec4F32Type(ids, types, words);
  if (vec4_type == 0u) {
    return 0u;
  }

  types->ptr_output_vec4_f32 = ids->Allocate();
  AppendInstruction(
      words, SpvOpTypePointer,
      {types->ptr_output_vec4_f32, static_cast<uint32_t>(SpvStorageClassOutput),
       vec4_type});
  return types->ptr_output_vec4_f32;
}

uint32_t GetOrCreateF32Constant(IdAllocator* ids, TypeCache* types,
                                std::unordered_map<uint32_t, uint32_t>* cache,
                                std::vector<uint32_t>* words, float value) {
  if (ids == nullptr || types == nullptr || cache == nullptr || words == nullptr) {
    return 0u;
  }

  const uint32_t bits = FloatToBits(value);
  auto iter = cache->find(bits);
  if (iter != cache->end()) {
    return iter->second;
  }

  const auto f32_type = GetOrCreateF32Type(ids, types, words);
  if (f32_type == 0u) {
    return 0u;
  }

  const auto const_id = ids->Allocate();
  AppendInstruction(words, SpvOpConstant, {f32_type, const_id, bits});
  cache->emplace(bits, const_id);
  return const_id;
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

  const auto& return_inst = entry_function->entry_block.instructions.front();
  const bool has_position_return =
      return_inst.has_return_value &&
      return_inst.return_value_kind == ir::ReturnValueKind::kConstVec4F32 &&
      module.stage == ir::PipelineStage::kVertex &&
      entry_function->return_builtin_position;

  if (return_inst.has_return_value && !has_position_return) {
    return false;
  }

  IdAllocator ids;
  TypeCache types;
  std::unordered_map<uint32_t, uint32_t> f32_constants;

  const uint32_t function_id = ids.Allocate();
  const uint32_t label_id = ids.Allocate();
  uint32_t position_output_var_id = 0u;
  uint32_t composite_return_id = 0u;

  std::vector<uint32_t> words;
  words.reserve(64u);

  words.push_back(SpvMagicNumber);
  words.push_back(kSpirvVersion13);
  words.push_back(0u);
  words.push_back(0u);  // id bound; backfilled after all ids are allocated
  words.push_back(0u);

  AppendInstruction(&words, SpvOpCapability,
                    {static_cast<uint32_t>(SpvCapabilityShader)});
  AppendInstruction(&words, SpvOpMemoryModel,
                    {static_cast<uint32_t>(SpvAddressingModelLogical),
                     static_cast<uint32_t>(SpvMemoryModelGLSL450)});
  if (has_position_return) {
    position_output_var_id = ids.Allocate();

    AppendEntryPoint(&words, execution_model, function_id, module.entry_point,
                     {position_output_var_id});
  } else {
    AppendEntryPoint(&words, execution_model, function_id, module.entry_point);
  }

  if (module.stage == ir::PipelineStage::kFragment) {
    AppendInstruction(
        &words, SpvOpExecutionMode,
        {function_id, static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
  }

  AppendName(&words, function_id, module.entry_point);
  if (has_position_return) {
    AppendName(&words, position_output_var_id, "position_output");
    AppendInstruction(
        &words, SpvOpDecorate,
        {position_output_var_id, static_cast<uint32_t>(SpvDecorationBuiltIn),
         static_cast<uint32_t>(SpvBuiltInPosition)});
  }
  const auto void_type_id = GetOrCreateVoidType(&ids, &types, &words);
  const auto function_type_id = GetOrCreateFunctionTypeVoid(&ids, &types, &words);
  if (void_type_id == 0u || function_type_id == 0u) {
    return false;
  }

  if (has_position_return) {
    const auto output_ptr_type_id =
        GetOrCreatePtrOutputVec4F32Type(&ids, &types, &words);
    if (output_ptr_type_id == 0u) {
      return false;
    }

    AppendInstruction(
        &words, SpvOpVariable,
        {output_ptr_type_id, position_output_var_id,
         static_cast<uint32_t>(SpvStorageClassOutput)});

    std::vector<uint32_t> constant_ids;
    constant_ids.reserve(4u);
    for (float value : return_inst.const_vec4_f32) {
      const auto const_id = GetOrCreateF32Constant(&ids, &types, &f32_constants,
                                                   &words, value);
      if (const_id == 0u) {
        return false;
      }
      constant_ids.push_back(const_id);
    }
  }
  AppendInstruction(
      &words, SpvOpFunction,
      {void_type_id, function_id, static_cast<uint32_t>(SpvFunctionControlMaskNone),
       function_type_id});
  AppendInstruction(&words, SpvOpLabel, {label_id});
  if (has_position_return) {
    const auto vec4_type_id = GetOrCreateVec4F32Type(&ids, &types, &words);
    if (vec4_type_id == 0u) {
      return false;
    }

    std::vector<uint32_t> constant_ids;
    constant_ids.reserve(4u);
    for (float value : return_inst.const_vec4_f32) {
      const auto const_id = GetOrCreateF32Constant(&ids, &types, &f32_constants,
                                                   &words, value);
      if (const_id == 0u) {
        return false;
      }
      constant_ids.push_back(const_id);
    }

    composite_return_id = ids.Allocate();
    AppendInstruction(&words, SpvOpCompositeConstruct,
                      {vec4_type_id, composite_return_id, constant_ids[0],
                       constant_ids[1], constant_ids[2], constant_ids[3]});
    AppendInstruction(&words, SpvOpStore, {position_output_var_id, composite_return_id});
  }
  AppendInstruction(&words, SpvOpReturn, {});
  AppendInstruction(&words, SpvOpFunctionEnd, {});

  words[3] = ids.Bound();

  result_ = PackBinaryModule(words);
  return true;
}

}  // namespace spirv
}  // namespace wgx
