// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter.h"

#include <array>
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

void AppendSection(std::vector<uint32_t>* dst, const std::vector<uint32_t>& src) {
  if (dst == nullptr || src.empty()) {
    return;
  }

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

struct SectionBuffers {
  std::vector<uint32_t> capabilities;
  std::vector<uint32_t> memory_model;
  std::vector<uint32_t> entry_points;
  std::vector<uint32_t> execution_modes;
  std::vector<uint32_t> debug;
  std::vector<uint32_t> annotations;
  std::vector<uint32_t> types_consts_globals;
  std::vector<uint32_t> functions;
};

class EmitContext {
 public:
  uint32_t AllocateId() { return ids_.Allocate(); }

  uint32_t Bound() const { return ids_.Bound(); }

  SectionBuffers& Sections() { return sections_; }

  uint32_t GetOrCreateVoidType() {
    if (types_.void_type != 0u) {
      return types_.void_type;
    }

    types_.void_type = AllocateId();
    AppendInstruction(&sections_.types_consts_globals, SpvOpTypeVoid,
                      {types_.void_type});
    return types_.void_type;
  }

  uint32_t GetOrCreateFunctionTypeVoid() {
    if (types_.function_void != 0u) {
      return types_.function_void;
    }

    const auto void_type = GetOrCreateVoidType();
    if (void_type == 0u) {
      return 0u;
    }

    types_.function_void = AllocateId();
    AppendInstruction(&sections_.types_consts_globals, SpvOpTypeFunction,
                      {types_.function_void, void_type});
    return types_.function_void;
  }

  uint32_t GetOrCreateF32Type() {
    if (types_.f32 != 0u) {
      return types_.f32;
    }

    types_.f32 = AllocateId();
    AppendInstruction(&sections_.types_consts_globals, SpvOpTypeFloat,
                      {types_.f32, 32u});
    return types_.f32;
  }

  uint32_t GetOrCreateVec4F32Type() {
    if (types_.vec4_f32 != 0u) {
      return types_.vec4_f32;
    }

    const auto f32_type = GetOrCreateF32Type();
    if (f32_type == 0u) {
      return 0u;
    }

    types_.vec4_f32 = AllocateId();
    AppendInstruction(&sections_.types_consts_globals, SpvOpTypeVector,
                      {types_.vec4_f32, f32_type, 4u});
    return types_.vec4_f32;
  }

  uint32_t GetOrCreatePtrOutputVec4F32Type() {
    if (types_.ptr_output_vec4_f32 != 0u) {
      return types_.ptr_output_vec4_f32;
    }

    const auto vec4_type = GetOrCreateVec4F32Type();
    if (vec4_type == 0u) {
      return 0u;
    }

    types_.ptr_output_vec4_f32 = AllocateId();
    AppendInstruction(&sections_.types_consts_globals, SpvOpTypePointer,
                      {types_.ptr_output_vec4_f32,
                       static_cast<uint32_t>(SpvStorageClassOutput),
                       vec4_type});
    return types_.ptr_output_vec4_f32;
  }

  uint32_t GetOrCreateF32Constant(float value) {
    const uint32_t bits = FloatToBits(value);
    auto iter = f32_constants_.find(bits);
    if (iter != f32_constants_.end()) {
      return iter->second;
    }

    const auto f32_type = GetOrCreateF32Type();
    if (f32_type == 0u) {
      return 0u;
    }

    const auto const_id = AllocateId();
    AppendInstruction(&sections_.types_consts_globals, SpvOpConstant,
                      {f32_type, const_id, bits});
    f32_constants_.emplace(bits, const_id);
    return const_id;
  }

 private:
  IdAllocator ids_;
  TypeCache types_;
  std::unordered_map<uint32_t, uint32_t> f32_constants_;
  SectionBuffers sections_;
};

const ir::Function* FindEntryFunction(const ir::Module& module) {
  for (const auto& function : module.functions) {
    if (function.name == module.entry_point) {
      return &function;
    }
  }

  return nullptr;
}

class ModuleBuilder {
 public:
  ModuleBuilder(const ir::Module& module, const ir::Function& entry,
                SpvExecutionModel execution_model)
      : module_(module), entry_(entry), execution_model_(execution_model) {}

  bool Build(EmitContext* ctx) {
    if (ctx == nullptr) {
      return false;
    }

    const auto& return_inst = entry_.entry_block.instructions.front();
    has_position_return_ = return_inst.has_return_value;

    if (!AllocateCoreIds(ctx)) {
      return false;
    }

    WriteCapabilityMemoryModel(ctx);
    WriteEntryPointSection(ctx);
    WriteExecutionModeSection(ctx);
    WriteDebugSection(ctx);
    WriteAnnotationSection(ctx);

    if (!WriteTypeConstGlobalSection(ctx)) {
      return false;
    }

    if (!WriteFunctionSection(ctx)) {
      return false;
    }

    return true;
  }

 private:
  bool AllocateCoreIds(EmitContext* ctx) {
    function_id_ = ctx->AllocateId();
    label_id_ = ctx->AllocateId();

    if (has_position_return_) {
      position_output_var_id_ = ctx->AllocateId();
    }

    return function_id_ != 0u && label_id_ != 0u;
  }

  void WriteCapabilityMemoryModel(EmitContext* ctx) {
    auto& sections = ctx->Sections();
    AppendInstruction(&sections.capabilities, SpvOpCapability,
                      {static_cast<uint32_t>(SpvCapabilityShader)});
    AppendInstruction(&sections.memory_model, SpvOpMemoryModel,
                      {static_cast<uint32_t>(SpvAddressingModelLogical),
                       static_cast<uint32_t>(SpvMemoryModelGLSL450)});
  }

  void WriteEntryPointSection(EmitContext* ctx) {
    auto& sections = ctx->Sections();
    if (has_position_return_) {
      AppendEntryPoint(&sections.entry_points, execution_model_, function_id_,
                       module_.entry_point, {position_output_var_id_});
    } else {
      AppendEntryPoint(&sections.entry_points, execution_model_, function_id_,
                       module_.entry_point);
    }
  }

  void WriteExecutionModeSection(EmitContext* ctx) {
    if (module_.stage != ir::PipelineStage::kFragment) {
      return;
    }

    auto& sections = ctx->Sections();
    AppendInstruction(
        &sections.execution_modes, SpvOpExecutionMode,
        {function_id_, static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
  }

  void WriteDebugSection(EmitContext* ctx) {
    auto& sections = ctx->Sections();
    AppendName(&sections.debug, function_id_, module_.entry_point);
    if (has_position_return_) {
      AppendName(&sections.debug, position_output_var_id_, "position_output");
    }
  }

  void WriteAnnotationSection(EmitContext* ctx) {
    if (!has_position_return_) {
      return;
    }

    auto& sections = ctx->Sections();
    AppendInstruction(&sections.annotations, SpvOpDecorate,
                      {position_output_var_id_,
                       static_cast<uint32_t>(SpvDecorationBuiltIn),
                       static_cast<uint32_t>(SpvBuiltInPosition)});
  }

  bool WriteTypeConstGlobalSection(EmitContext* ctx) {
    void_type_id_ = ctx->GetOrCreateVoidType();
    function_type_id_ = ctx->GetOrCreateFunctionTypeVoid();
    if (void_type_id_ == 0u || function_type_id_ == 0u) {
      return false;
    }

    if (!has_position_return_) {
      return true;
    }

    const auto output_ptr_type_id = ctx->GetOrCreatePtrOutputVec4F32Type();
    vec4_type_id_ = ctx->GetOrCreateVec4F32Type();
    if (output_ptr_type_id == 0u || vec4_type_id_ == 0u) {
      return false;
    }

    auto& sections = ctx->Sections();
    AppendInstruction(&sections.types_consts_globals, SpvOpVariable,
                      {output_ptr_type_id, position_output_var_id_,
                       static_cast<uint32_t>(SpvStorageClassOutput)});

    const auto& return_value =
        entry_.entry_block.instructions.front().const_vec4_f32;
    for (size_t i = 0; i < position_const_ids_.size(); ++i) {
      const auto const_id = ctx->GetOrCreateF32Constant(return_value[i]);
      if (const_id == 0u) {
        return false;
      }
      position_const_ids_[i] = const_id;
    }

    return true;
  }

  bool WriteFunctionSection(EmitContext* ctx) {
    auto& section = ctx->Sections().functions;

    AppendInstruction(
        &section, SpvOpFunction,
        {void_type_id_, function_id_,
         static_cast<uint32_t>(SpvFunctionControlMaskNone), function_type_id_});
    AppendInstruction(&section, SpvOpLabel, {label_id_});

    if (has_position_return_) {
      composite_return_id_ = ctx->AllocateId();
      AppendInstruction(
          &section, SpvOpCompositeConstruct,
          {vec4_type_id_, composite_return_id_, position_const_ids_[0],
           position_const_ids_[1], position_const_ids_[2],
           position_const_ids_[3]});
      AppendInstruction(&section, SpvOpStore,
                        {position_output_var_id_, composite_return_id_});
    }

    AppendInstruction(&section, SpvOpReturn, {});
    AppendInstruction(&section, SpvOpFunctionEnd, {});

    return true;
  }

 private:
  const ir::Module& module_;
  const ir::Function& entry_;
  SpvExecutionModel execution_model_;

  bool has_position_return_ = false;

  uint32_t function_id_ = 0u;
  uint32_t label_id_ = 0u;
  uint32_t position_output_var_id_ = 0u;
  uint32_t composite_return_id_ = 0u;
  uint32_t void_type_id_ = 0u;
  uint32_t function_type_id_ = 0u;
  uint32_t vec4_type_id_ = 0u;
  std::array<uint32_t, 4> position_const_ids_ = {0u, 0u, 0u, 0u};
};

}  // namespace

bool Emitter::Emit(const ir::Module& module) {
  result_.clear();

  if (module.entry_point.empty() || module.stage == ir::PipelineStage::kUnknown) {
    return false;
  }

  if (module.functions.empty()) {
    return false;
  }

  const ir::Function* entry_function = FindEntryFunction(module);
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

  EmitContext context;
  ModuleBuilder builder(module, *entry_function, execution_model);
  if (!builder.Build(&context)) {
    return false;
  }

  std::vector<uint32_t> words;
  words.reserve(128u);
  words.push_back(SpvMagicNumber);
  words.push_back(kSpirvVersion13);
  words.push_back(0u);
  words.push_back(0u);  // id bound; backfilled after all ids are allocated
  words.push_back(0u);

  const auto& sections = context.Sections();
  AppendSection(&words, sections.capabilities);
  AppendSection(&words, sections.memory_model);
  AppendSection(&words, sections.entry_points);
  AppendSection(&words, sections.execution_modes);
  AppendSection(&words, sections.debug);
  AppendSection(&words, sections.annotations);
  AppendSection(&words, sections.types_consts_globals);
  AppendSection(&words, sections.functions);

  words[3] = context.Bound();

  result_ = PackBinaryModule(words);
  return true;
}

}  // namespace spirv
}  // namespace wgx
