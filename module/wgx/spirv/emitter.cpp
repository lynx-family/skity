// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
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
                      std::initializer_list<uint32_t> interfaces = {}) {
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
  if (function.entry_block.instructions.empty()) {
    return false;
  }
  const auto& last_inst = function.entry_block.instructions.back();
  if (last_inst.kind != ir::InstKind::kReturn) {
    return false;
  }
  if (last_inst.has_return_value) {
    if (function.stage != ir::PipelineStage::kVertex ||
        !function.return_builtin_position) {
      return false;
    }
    if (last_inst.return_value_kind != ir::ReturnValueKind::kConstVec4F32 &&
        last_inst.return_value_kind != ir::ReturnValueKind::kVariableRef) {
      return false;
    }
  }
  for (const auto& inst : function.entry_block.instructions) {
    switch (inst.kind) {
      case ir::InstKind::kReturn:
      case ir::InstKind::kVariable:
      case ir::InstKind::kStore:
        break;
      default:
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

class IdAllocator {
 public:
  uint32_t Allocate() { return next_id_++; }
  uint32_t Bound() const { return next_id_; }

 private:
  uint32_t next_id_ = 1u;
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

class TypeEmitter {
 public:
  TypeEmitter(IdAllocator* ids, SectionBuffers* sections,
              ir::TypeTable* type_table)
      : ids_(ids), sections_(sections), type_table_(type_table) {}

  ir::TypeTable* GetTypeTable() { return type_table_; }
  IdAllocator* GetIds() { return ids_; }

  uint32_t EmitType(ir::TypeId type_id) {
    if (type_id == ir::kInvalidTypeId) return 0;

    auto it = emitted_types_.find(type_id);
    if (it != emitted_types_.end()) {
      return it->second;
    }

    const ir::Type* type = type_table_->GetType(type_id);
    if (type == nullptr) return 0;

    uint32_t spirv_id = ids_->Allocate();
    emitted_types_[type_id] = spirv_id;

    switch (type->kind) {
      case ir::TypeKind::kVoid:
        AppendInstruction(&sections_->types_consts_globals, SpvOpTypeVoid,
                          {spirv_id});
        break;
      case ir::TypeKind::kBool:
        AppendInstruction(&sections_->types_consts_globals, SpvOpTypeBool,
                          {spirv_id});
        break;
      case ir::TypeKind::kI32:
        AppendInstruction(&sections_->types_consts_globals, SpvOpTypeInt,
                          {spirv_id, 32u, 1u});
        break;
      case ir::TypeKind::kU32:
        AppendInstruction(&sections_->types_consts_globals, SpvOpTypeInt,
                          {spirv_id, 32u, 0u});
        break;
      case ir::TypeKind::kF32:
        AppendInstruction(&sections_->types_consts_globals, SpvOpTypeFloat,
                          {spirv_id, 32u});
        break;
      case ir::TypeKind::kVector:
        EmitVectorType(type, spirv_id);
        break;
      case ir::TypeKind::kMatrix:
        EmitMatrixType(type, spirv_id);
        break;
      case ir::TypeKind::kArray:
        EmitArrayType(type, spirv_id);
        break;
      case ir::TypeKind::kStruct:
        EmitStructType(type, spirv_id);
        break;
      case ir::TypeKind::kPointer:
        EmitPointerType(type, spirv_id);
        break;
    }

    return spirv_id;
  }

  uint32_t GetFunctionTypeVoid() {
    if (function_void_type_ != 0) return function_void_type_;

    uint32_t void_type = EmitType(type_table_->GetVoidType());
    if (void_type == 0) return 0;

    function_void_type_ = ids_->Allocate();
    AppendInstruction(&sections_->types_consts_globals, SpvOpTypeFunction,
                      {function_void_type_, void_type});
    return function_void_type_;
  }

  uint32_t GetPointerType(ir::TypeId pointee, SpvStorageClass storage) {
    auto key = std::make_pair(pointee, storage);
    auto it = pointer_types_.find(key);
    if (it != pointer_types_.end()) {
      return it->second;
    }

    uint32_t pointee_id = EmitType(pointee);
    if (pointee_id == 0) return 0;

    uint32_t ptr_id = ids_->Allocate();
    AppendInstruction(&sections_->types_consts_globals, SpvOpTypePointer,
                      {ptr_id, static_cast<uint32_t>(storage), pointee_id});
    pointer_types_[key] = ptr_id;
    return ptr_id;
  }

  uint32_t EmitF32Constant(float value) {
    uint32_t bits = FloatToBits(value);
    auto it = f32_constants_.find(bits);
    if (it != f32_constants_.end()) {
      return it->second;
    }

    uint32_t f32_type = EmitType(type_table_->GetF32Type());
    if (f32_type == 0) return 0;

    uint32_t const_id = ids_->Allocate();
    AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                      {f32_type, const_id, bits});
    f32_constants_[bits] = const_id;
    return const_id;
  }

 private:
  void EmitVectorType(const ir::Type* type, uint32_t spirv_id) {
    uint32_t component_id = EmitType(type->element_type);
    if (component_id == 0) return;
    AppendInstruction(&sections_->types_consts_globals, SpvOpTypeVector,
                      {spirv_id, component_id, type->count});
  }

  void EmitMatrixType(const ir::Type* type, uint32_t spirv_id) {
    uint32_t component_id = EmitType(type->element_type);
    if (component_id == 0) return;
    AppendInstruction(&sections_->types_consts_globals, SpvOpTypeMatrix,
                      {spirv_id, component_id, type->count2});
  }

  void EmitArrayType(const ir::Type* type, uint32_t spirv_id) {
    uint32_t element_id = EmitType(type->element_type);
    if (element_id == 0) return;
    uint32_t size_const = ids_->Allocate();
    uint32_t u32_type = EmitType(type_table_->GetU32Type());
    AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                      {u32_type, size_const, type->count});
    AppendInstruction(&sections_->types_consts_globals, SpvOpTypeArray,
                      {spirv_id, element_id, size_const});
  }

  void EmitStructType(const ir::Type* type, uint32_t spirv_id) {
    std::vector<uint32_t> operands;
    operands.push_back(spirv_id);
    for (const auto& member : type->members) {
      uint32_t member_type_id = EmitType(member.type);
      if (member_type_id == 0) return;
      operands.push_back(member_type_id);
    }
    AppendInstruction(&sections_->types_consts_globals, SpvOpTypeStruct,
                      operands);
  }

  void EmitPointerType(const ir::Type* type, uint32_t spirv_id) {
    uint32_t pointee_id = EmitType(type->element_type);
    if (pointee_id == 0) return;
    AppendInstruction(
        &sections_->types_consts_globals, SpvOpTypePointer,
        {spirv_id,
         static_cast<uint32_t>(ToSpvStorageClass(type->storage_class)),
         pointee_id});
  }

  struct PairHash {
    size_t operator()(const std::pair<ir::TypeId, SpvStorageClass>& p) const {
      return std::hash<ir::TypeId>{}(p.first) ^
             (std::hash<int>{}(static_cast<int>(p.second)) << 1);
    }
  };

  IdAllocator* ids_;
  SectionBuffers* sections_;
  ir::TypeTable* type_table_;
  std::unordered_map<ir::TypeId, uint32_t> emitted_types_;
  std::unordered_map<uint32_t, uint32_t> f32_constants_;
  std::unordered_map<std::pair<ir::TypeId, SpvStorageClass>, uint32_t, PairHash>
      pointer_types_;
  uint32_t function_void_type_ = 0;
};

const ir::Function* FindEntryFunction(const ir::Module& module) {
  for (const auto& function : module.functions) {
    if (function.name == module.entry_point) {
      return &function;
    }
  }
  return nullptr;
}

struct LocalVarInfo {
  uint32_t ir_var_id = 0;
  ir::TypeId var_type = ir::kInvalidTypeId;
  uint32_t spirv_var_id = 0;
};

class ModuleBuilder {
 public:
  ModuleBuilder(const ir::Module& module, const ir::Function& entry,
                SpvExecutionModel execution_model)
      : module_(module), entry_(entry), execution_model_(execution_model) {}

  bool Build(SectionBuffers* sections, std::vector<uint32_t>* output_words) {
    if (sections == nullptr || output_words == nullptr) return false;

    sections_ = sections;

    ir::TypeTable* type_table = module_.type_table.get();
    if (type_table == nullptr) return false;

    type_emitter_ = std::make_unique<TypeEmitter>(&ids_, sections_, type_table);

    if (!AnalyzeEntryBlock()) return false;
    if (!AllocateCoreIds()) return false;

    WriteCapabilityMemoryModel();
    WriteEntryPointSection();
    WriteExecutionModeSection();
    WriteDebugSection();
    WriteAnnotationSection();
    if (!WriteTypeConstGlobalSection()) return false;
    if (!WriteFunctionSection()) return false;

    AssembleModule(output_words);
    return true;
  }

 private:
  bool AnalyzeEntryBlock() {
    const auto& instructions = entry_.entry_block.instructions;
    if (instructions.empty()) return false;

    const auto& last_inst = instructions.back();
    if (last_inst.kind != ir::InstKind::kReturn) return false;

    has_position_return_ = last_inst.has_return_value;

    for (const auto& inst : instructions) {
      if (inst.kind == ir::InstKind::kVariable) {
        LocalVarInfo info;
        info.ir_var_id = inst.result_id;
        info.var_type = inst.result_type;
        local_vars_.push_back(info);
      }
    }
    return true;
  }

  bool AllocateCoreIds() {
    function_id_ = ids_.Allocate();
    label_id_ = ids_.Allocate();
    if (has_position_return_) {
      position_output_var_id_ = ids_.Allocate();
    }
    for (auto& var : local_vars_) {
      var.spirv_var_id = ids_.Allocate();
    }
    return function_id_ != 0 && label_id_ != 0;
  }

  void WriteCapabilityMemoryModel() {
    AppendInstruction(&sections_->capabilities, SpvOpCapability,
                      {static_cast<uint32_t>(SpvCapabilityShader)});
    AppendInstruction(&sections_->memory_model, SpvOpMemoryModel,
                      {static_cast<uint32_t>(SpvAddressingModelLogical),
                       static_cast<uint32_t>(SpvMemoryModelGLSL450)});
  }

  void WriteEntryPointSection() {
    if (has_position_return_) {
      AppendEntryPoint(&sections_->entry_points, execution_model_, function_id_,
                       module_.entry_point, {position_output_var_id_});
    } else {
      AppendEntryPoint(&sections_->entry_points, execution_model_, function_id_,
                       module_.entry_point);
    }
  }

  void WriteExecutionModeSection() {
    if (module_.stage != ir::PipelineStage::kFragment) return;
    AppendInstruction(
        &sections_->execution_modes, SpvOpExecutionMode,
        {function_id_, static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
  }

  void WriteDebugSection() {
    AppendName(&sections_->debug, function_id_, module_.entry_point);
    if (has_position_return_) {
      AppendName(&sections_->debug, position_output_var_id_, "position_output");
    }
    for (const auto& inst : entry_.entry_block.instructions) {
      if (inst.kind == ir::InstKind::kVariable && !inst.var_name.empty()) {
        auto it = FindLocalVar(inst.result_id);
        if (it != local_vars_.end()) {
          AppendName(&sections_->debug, it->spirv_var_id, inst.var_name);
        }
      }
    }
  }

  void WriteAnnotationSection() {
    if (!has_position_return_) return;
    AppendInstruction(
        &sections_->annotations, SpvOpDecorate,
        {position_output_var_id_, static_cast<uint32_t>(SpvDecorationBuiltIn),
         static_cast<uint32_t>(SpvBuiltInPosition)});
  }

  bool WriteTypeConstGlobalSection() {
    uint32_t void_type =
        type_emitter_->EmitType(type_emitter_->GetTypeTable()->GetVoidType());
    if (void_type == 0) return false;

    function_type_id_ = type_emitter_->GetFunctionTypeVoid();
    if (function_type_id_ == 0) return false;

    if (!has_position_return_) return true;

    ir::TypeId vec4_type = type_emitter_->GetTypeTable()->GetVectorType(
        type_emitter_->GetTypeTable()->GetF32Type(), 4);
    if (vec4_type == ir::kInvalidTypeId) return false;

    vec4_type_id_ = type_emitter_->EmitType(vec4_type);
    if (vec4_type_id_ == 0) return false;

    uint32_t output_ptr_type =
        type_emitter_->GetPointerType(vec4_type, SpvStorageClassOutput);
    if (output_ptr_type == 0) return false;

    AppendInstruction(&sections_->types_consts_globals, SpvOpVariable,
                      {output_ptr_type, position_output_var_id_,
                       static_cast<uint32_t>(SpvStorageClassOutput)});

    const auto& return_inst = entry_.entry_block.instructions.back();
    if (return_inst.return_value_kind == ir::ReturnValueKind::kConstVec4F32) {
      for (size_t i = 0; i < position_const_ids_.size(); ++i) {
        position_const_ids_[i] =
            type_emitter_->EmitF32Constant(return_inst.const_vec4_f32[i]);
        if (position_const_ids_[i] == 0) return false;
      }
    }

    return true;
  }

  bool WriteFunctionSection() {
    uint32_t void_type =
        type_emitter_->EmitType(type_emitter_->GetTypeTable()->GetVoidType());

    AppendInstruction(
        &sections_->functions, SpvOpFunction,
        {void_type, function_id_,
         static_cast<uint32_t>(SpvFunctionControlMaskNone), function_type_id_});
    AppendInstruction(&sections_->functions, SpvOpLabel, {label_id_});

    for (auto& var : local_vars_) {
      uint32_t ptr_type =
          type_emitter_->GetPointerType(var.var_type, SpvStorageClassFunction);
      if (ptr_type == 0) return false;
      AppendInstruction(&sections_->functions, SpvOpVariable,
                        {ptr_type, var.spirv_var_id,
                         static_cast<uint32_t>(SpvStorageClassFunction)});
    }

    for (const auto& inst : entry_.entry_block.instructions) {
      if (!EmitInstruction(inst)) return false;
    }

    AppendInstruction(&sections_->functions, SpvOpFunctionEnd, {});
    return true;
  }

  bool EmitInstruction(const ir::Instruction& inst) {
    switch (inst.kind) {
      case ir::InstKind::kVariable:
        return true;
      case ir::InstKind::kStore:
        return EmitStore(inst);
      case ir::InstKind::kReturn:
        return EmitReturn(inst);
      default:
        return false;
    }
  }

  bool EmitStore(const ir::Instruction& inst) {
    if (inst.operands.size() < 2) return false;
    if (inst.operands[0].kind != ir::Operand::Kind::kId) return false;

    uint32_t ir_var_id = inst.operands[0].id;
    auto var_it = FindLocalVar(ir_var_id);
    if (var_it == local_vars_.end()) return false;

    uint32_t value_id = 0;
    if (inst.operands.size() == 2 &&
        inst.operands[1].kind == ir::Operand::Kind::kId) {
      auto src_it = FindLocalVar(inst.operands[1].id);
      if (src_it == local_vars_.end()) return false;
      value_id = ids_.Allocate();
      AppendInstruction(&sections_->functions, SpvOpLoad,
                        {vec4_type_id_, value_id, src_it->spirv_var_id});
    } else if (inst.operands.size() == 5) {
      std::array<uint32_t, 4> const_ids;
      for (size_t i = 0; i < 4; ++i) {
        if (inst.operands[i + 1].kind != ir::Operand::Kind::kConstF32) {
          return false;
        }
        const_ids[i] =
            type_emitter_->EmitF32Constant(inst.operands[i + 1].const_f32);
        if (const_ids[i] == 0) return false;
      }

      value_id = ids_.Allocate();
      AppendInstruction(&sections_->functions, SpvOpCompositeConstruct,
                        {vec4_type_id_, value_id, const_ids[0], const_ids[1],
                         const_ids[2], const_ids[3]});
    } else {
      return false;
    }

    AppendInstruction(&sections_->functions, SpvOpStore,
                      {var_it->spirv_var_id, value_id});
    return true;
  }

  bool EmitReturn(const ir::Instruction& inst) {
    if (!inst.has_return_value) {
      AppendInstruction(&sections_->functions, SpvOpReturn, {});
      return true;
    }

    uint32_t value_to_store = 0;

    if (inst.return_value_kind == ir::ReturnValueKind::kConstVec4F32) {
      value_to_store = ids_.Allocate();
      AppendInstruction(&sections_->functions, SpvOpCompositeConstruct,
                        {vec4_type_id_, value_to_store, position_const_ids_[0],
                         position_const_ids_[1], position_const_ids_[2],
                         position_const_ids_[3]});
    } else if (inst.return_value_kind == ir::ReturnValueKind::kVariableRef) {
      auto var_it = FindLocalVar(inst.var_id);
      if (var_it == local_vars_.end()) return false;
      value_to_store = ids_.Allocate();
      AppendInstruction(&sections_->functions, SpvOpLoad,
                        {vec4_type_id_, value_to_store, var_it->spirv_var_id});
    } else {
      return false;
    }

    AppendInstruction(&sections_->functions, SpvOpStore,
                      {position_output_var_id_, value_to_store});
    AppendInstruction(&sections_->functions, SpvOpReturn, {});
    return true;
  }

  void AssembleModule(std::vector<uint32_t>* output_words) {
    output_words->reserve(128);
    output_words->push_back(SpvMagicNumber);
    output_words->push_back(kSpirvVersion13);
    output_words->push_back(0u);
    output_words->push_back(0u);
    output_words->push_back(0u);

    AppendSection(output_words, sections_->capabilities);
    AppendSection(output_words, sections_->memory_model);
    AppendSection(output_words, sections_->entry_points);
    AppendSection(output_words, sections_->execution_modes);
    AppendSection(output_words, sections_->debug);
    AppendSection(output_words, sections_->annotations);
    AppendSection(output_words, sections_->types_consts_globals);
    AppendSection(output_words, sections_->functions);

    (*output_words)[3] = ids_.Bound();
  }

  std::vector<LocalVarInfo>::iterator FindLocalVar(uint32_t ir_var_id) {
    for (auto it = local_vars_.begin(); it != local_vars_.end(); ++it) {
      if (it->ir_var_id == ir_var_id) return it;
    }
    return local_vars_.end();
  }

 private:
  const ir::Module& module_;
  const ir::Function& entry_;
  SpvExecutionModel execution_model_;

  IdAllocator ids_;
  SectionBuffers* sections_ = nullptr;
  std::unique_ptr<TypeEmitter> type_emitter_;

  bool has_position_return_ = false;
  uint32_t function_id_ = 0;
  uint32_t label_id_ = 0;
  uint32_t position_output_var_id_ = 0;
  uint32_t function_type_id_ = 0;
  uint32_t vec4_type_id_ = 0;
  std::array<uint32_t, 4> position_const_ids_ = {0, 0, 0, 0};
  std::vector<LocalVarInfo> local_vars_;
};

}  // namespace

bool Emitter::Emit(const ir::Module& module) {
  result_.clear();

  if (module.entry_point.empty() ||
      module.stage == ir::PipelineStage::kUnknown) {
    return false;
  }
  if (module.functions.empty()) return false;

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
