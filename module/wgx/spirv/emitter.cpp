// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "spirv/emitter.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/verifier.h"
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
                      const std::vector<uint32_t>& interfaces = {}) {
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

/**
 * Check if the backend supports the given IR function.
 *
 * This performs backend-specific capability checks on top of structural
 * validation. Structural validation is done via ir::Verifier.
 */
bool SupportsCurrentIR(const ir::Function& function) {
  // First, run structural validation
  auto result = ir::Verify(function);
  if (!result.valid) {
    return false;
  }

  // Backend-specific capability checks
  // Currently only support:
  // - void returns (empty output_vars)
  // - single @builtin(position) in vertex shaders
  // - straight-line code plus selection control flow for if/if-else
  if (!function.output_vars.empty()) {
    if (function.stage != ir::PipelineStage::kVertex) {
      return false;  // Only vertex shaders supported for returns
    }
    if (function.output_vars.size() != 1) {
      return false;  // Only single output supported currently
    }
    const auto& output = function.output_vars[0];
    if (output.decoration_kind != ir::OutputDecorationKind::kBuiltin ||
        output.GetBuiltin() != ir::BuiltinType::kPosition) {
      return false;  // Only @builtin(position) supported
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

  uint32_t EmitI32Constant(int32_t value) {
    auto it = i32_constants_.find(value);
    if (it != i32_constants_.end()) {
      return it->second;
    }

    uint32_t i32_type = EmitType(type_table_->GetI32Type());
    if (i32_type == 0) return 0;

    uint32_t const_id = ids_->Allocate();
    AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                      {i32_type, const_id, static_cast<uint32_t>(value)});
    i32_constants_[value] = const_id;
    return const_id;
  }

  uint32_t EmitU32Constant(uint32_t value) {
    auto it = u32_constants_.find(value);
    if (it != u32_constants_.end()) {
      return it->second;
    }

    uint32_t u32_type = EmitType(type_table_->GetU32Type());
    if (u32_type == 0) return 0;

    uint32_t const_id = ids_->Allocate();
    AppendInstruction(&sections_->types_consts_globals, SpvOpConstant,
                      {u32_type, const_id, value});
    u32_constants_[value] = const_id;
    return const_id;
  }

  uint32_t EmitBoolConstant(bool value) {
    auto it = bool_constants_.find(value);
    if (it != bool_constants_.end()) {
      return it->second;
    }

    uint32_t bool_type = EmitType(type_table_->GetBoolType());
    if (bool_type == 0) return 0;

    uint32_t const_id = ids_->Allocate();
    auto opcode = value ? SpvOpConstantTrue : SpvOpConstantFalse;
    AppendInstruction(&sections_->types_consts_globals, opcode,
                      {bool_type, const_id});
    bool_constants_[value] = const_id;
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
  std::unordered_map<int32_t, uint32_t> i32_constants_;
  std::unordered_map<uint32_t, uint32_t> u32_constants_;
  std::unordered_map<bool, uint32_t> bool_constants_;
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

struct GlobalVarInfo {
  uint32_t ir_var_id = 0;
  ir::TypeId var_type =
      ir::kInvalidTypeId;  // SPIR-V variable type (may be struct)
  ir::TypeId inner_type =
      ir::kInvalidTypeId;  // Original inner type for buffer access
  uint32_t spirv_var_id = 0;
  SpvStorageClass storage_class = SpvStorageClassPrivate;
  std::optional<ir::Value> initializer;  // Constant initializer, if any
  std::optional<uint32_t> group;
  std::optional<uint32_t> binding;
  bool is_wrapped_buffer = false;  // True if var_type is a wrapper struct
};

struct ValueInfo {
  uint32_t ir_value_id = 0;
  ir::TypeId value_type = ir::kInvalidTypeId;
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
    const ir::Block* entry_block = entry_.GetBlock(entry_.entry_block_id);
    if (entry_block == nullptr || entry_.blocks.empty()) return false;

    // Initialize output variables from IR
    for (const auto& ir_output : entry_.output_vars) {
      OutputVarInfo info;
      info.name = ir_output.name;
      info.ir_type = ir_output.type;
      info.decoration_kind = ir_output.decoration_kind;
      info.decoration_value = ir_output.decoration_value;
      output_vars_.push_back(std::move(info));
    }

    // Collect local variables and SSA values from all blocks
    for (const auto& block : entry_.blocks) {
      for (const auto& inst : block.instructions) {
        if (inst.kind == ir::InstKind::kVariable) {
          LocalVarInfo info;
          info.ir_var_id = inst.var_id;
          info.var_type = inst.result_type;
          local_vars_.push_back(info);
        } else if (inst.kind == ir::InstKind::kBinary ||
                   inst.kind == ir::InstKind::kLoad) {
          ValueInfo info;
          info.ir_value_id = inst.result_id;
          info.value_type = inst.result_type;
          values_.push_back(info);
        }
      }
    }

    CollectGlobalVarReferences(entry_.blocks);

    return true;
  }

  /**
   * Collects global variable references from instructions.
   * Global variables are referenced by Load/Store but not declared by
   * kVariable.
   */
  void CollectGlobalVarReferences(const std::vector<ir::Block>& blocks) {
    std::unordered_set<uint32_t> local_var_ids;
    for (const auto& var : local_vars_) {
      local_var_ids.insert(var.ir_var_id);
    }

    for (const auto& block : blocks) {
      for (const auto& inst : block.instructions) {
        for (const auto& operand : inst.operands) {
          if (!operand.IsVariable()) {
            continue;
          }

          uint32_t var_id = operand.GetVarId().value_or(0);
          ir::TypeId var_type = operand.type;
          if (var_id == 0 || var_type == ir::kInvalidTypeId) {
            continue;
          }
          if (local_var_ids.count(var_id) > 0) {
            continue;
          }

          bool already_tracked = false;
          for (const auto& global : global_vars_) {
            if (global.ir_var_id == var_id) {
              already_tracked = true;
              break;
            }
          }
          if (already_tracked) {
            continue;
          }

          GlobalVarInfo info;
          info.ir_var_id = var_id;
          info.var_type = var_type;

          auto global_it = module_.global_variables.find(var_id);
          if (global_it != module_.global_variables.end()) {
            info.storage_class =
                ToSpvStorageClass(global_it->second.storage_class);
            info.initializer = global_it->second.initializer;
            info.group = global_it->second.group;
            info.binding = global_it->second.binding;
            if (global_it->second.inner_type != ir::kInvalidTypeId) {
              info.inner_type = global_it->second.inner_type;
              info.is_wrapped_buffer = true;
              info.var_type = global_it->second.type;
            }
          } else {
            info.storage_class = SpvStorageClassPrivate;
          }

          global_vars_.push_back(info);
        }
      }
    }
  }

  bool AllocateCoreIds() {
    function_id_ = ids_.Allocate();
    for (auto& output : output_vars_) {
      output.spirv_var_id = ids_.Allocate();
    }
    for (auto& var : local_vars_) {
      var.spirv_var_id = ids_.Allocate();
    }
    for (auto& global : global_vars_) {
      global.spirv_var_id = ids_.Allocate();
    }
    for (const auto& block : entry_.blocks) {
      block_label_map_[block.id] = ids_.Allocate();
    }
    return function_id_ != 0;
  }

  void WriteCapabilityMemoryModel() {
    AppendInstruction(&sections_->capabilities, SpvOpCapability,
                      {static_cast<uint32_t>(SpvCapabilityShader)});
    AppendInstruction(&sections_->memory_model, SpvOpMemoryModel,
                      {static_cast<uint32_t>(SpvAddressingModelLogical),
                       static_cast<uint32_t>(SpvMemoryModelGLSL450)});
  }

  void WriteEntryPointSection() {
    std::vector<uint32_t> interface_ids;
    for (const auto& output : output_vars_) {
      interface_ids.push_back(output.spirv_var_id);
    }
    AppendEntryPoint(&sections_->entry_points, execution_model_, function_id_,
                     module_.entry_point, interface_ids);
  }

  void WriteExecutionModeSection() {
    if (module_.stage != ir::PipelineStage::kFragment) return;
    AppendInstruction(
        &sections_->execution_modes, SpvOpExecutionMode,
        {function_id_, static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
  }

  void WriteDebugSection() {
    AppendName(&sections_->debug, function_id_, module_.entry_point);
    for (const auto& output : output_vars_) {
      AppendName(&sections_->debug, output.spirv_var_id, output.name);
    }
    for (const auto& block : entry_.blocks) {
      for (const auto& inst : block.instructions) {
        if (inst.kind == ir::InstKind::kVariable && !inst.var_name.empty()) {
          auto it = FindLocalVar(inst.var_id);
          if (it != local_vars_.end()) {
            AppendName(&sections_->debug, it->spirv_var_id, inst.var_name);
          }
        }
      }
    }
  }

  /**
   * Maps IR builtin type to SPIR-V SpvBuiltIn value.
   */
  static SpvBuiltIn ConvertBuiltin(ir::BuiltinType builtin) {
    switch (builtin) {
      case ir::BuiltinType::kPosition:
        return SpvBuiltInPosition;
      case ir::BuiltinType::kNone:
      default:
        return SpvBuiltInMax;
    }
  }

  void WriteAnnotationSection() {
    for (const auto& output : output_vars_) {
      if (output.decoration_kind == ir::OutputDecorationKind::kBuiltin) {
        SpvBuiltIn spirv_builtin = ConvertBuiltin(output.GetBuiltin());
        AppendInstruction(
            &sections_->annotations, SpvOpDecorate,
            {output.spirv_var_id, static_cast<uint32_t>(SpvDecorationBuiltIn),
             static_cast<uint32_t>(spirv_builtin)});
      } else if (output.decoration_kind ==
                 ir::OutputDecorationKind::kLocation) {
        AppendInstruction(
            &sections_->annotations, SpvOpDecorate,
            {output.spirv_var_id, static_cast<uint32_t>(SpvDecorationLocation),
             output.decoration_value});
      }
    }
    for (const auto& global : global_vars_) {
      if (global.group.has_value()) {
        AppendInstruction(&sections_->annotations, SpvOpDecorate,
                          {global.spirv_var_id,
                           static_cast<uint32_t>(SpvDecorationDescriptorSet),
                           global.group.value()});
      }
      if (global.binding.has_value()) {
        AppendInstruction(
            &sections_->annotations, SpvOpDecorate,
            {global.spirv_var_id, static_cast<uint32_t>(SpvDecorationBinding),
             global.binding.value()});
      }
      // For wrapped buffers, add Block and Offset decorations
      if (global.is_wrapped_buffer) {
        uint32_t struct_type_id = type_emitter_->EmitType(global.var_type);
        if (struct_type_id != 0) {
          AppendInstruction(
              &sections_->annotations, SpvOpDecorate,
              {struct_type_id, static_cast<uint32_t>(SpvDecorationBlock)});
        }
        // Get the member offset from the struct type
        const ir::Type* struct_type =
            type_emitter_->GetTypeTable()->GetType(global.var_type);
        uint32_t member_offset = 0;
        if (struct_type != nullptr && !struct_type->members.empty()) {
          member_offset = struct_type->members[0].offset;
        }
        AppendInstruction(
            &sections_->annotations, SpvOpMemberDecorate,
            {struct_type_id, 0, static_cast<uint32_t>(SpvDecorationOffset),
             member_offset});
      }
    }
  }

  bool WriteTypeConstGlobalSection() {
    uint32_t void_type =
        type_emitter_->EmitType(type_emitter_->GetTypeTable()->GetVoidType());
    if (void_type == 0) return false;

    function_type_id_ = type_emitter_->GetFunctionTypeVoid();
    if (function_type_id_ == 0) return false;

    // Create OpVariable for each output
    for (auto& output : output_vars_) {
      // For now, get type from the output variable's type
      // Future: handle struct member extraction
      uint32_t spirv_type = type_emitter_->EmitType(output.ir_type);
      if (spirv_type == 0) return false;

      uint32_t output_ptr_type =
          type_emitter_->GetPointerType(output.ir_type, SpvStorageClassOutput);
      if (output_ptr_type == 0) return false;

      AppendInstruction(&sections_->types_consts_globals, SpvOpVariable,
                        {output_ptr_type, output.spirv_var_id,
                         static_cast<uint32_t>(SpvStorageClassOutput)});
    }

    // Create OpVariable for each global variable
    for (auto& global : global_vars_) {
      uint32_t spirv_type = type_emitter_->EmitType(global.var_type);
      if (spirv_type == 0) return false;

      uint32_t global_ptr_type =
          type_emitter_->GetPointerType(global.var_type, global.storage_class);
      if (global_ptr_type == 0) return false;

      std::vector<uint32_t> operands = {
          global_ptr_type, global.spirv_var_id,
          static_cast<uint32_t>(global.storage_class)};

      // Add initializer if present
      if (global.initializer.has_value()) {
        uint32_t init_id = 0;
        if (global.is_wrapped_buffer) {
          // For wrapped buffers, wrap the initializer in a struct
          init_id =
              EmitWrappedConstant(global.initializer.value(), global.var_type);
        } else {
          init_id = EmitConstant(global.initializer.value());
        }
        if (init_id != 0) {
          operands.push_back(init_id);
        }
      }

      AppendInstruction(&sections_->types_consts_globals, SpvOpVariable,
                        operands);
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

    for (const auto& block : entry_.blocks) {
      uint32_t block_label_id = GetOrCreateBlockLabel(block.id);
      AppendInstruction(&sections_->functions, SpvOpLabel, {block_label_id});

      // Emit local variable declarations at the start of the entry block
      if (block.id == entry_.entry_block_id) {
        for (auto& var : local_vars_) {
          uint32_t ptr_type =
              type_emitter_->GetPointerType(var.var_type, SpvStorageClassFunction);
          if (ptr_type == 0) return false;
          AppendInstruction(&sections_->functions, SpvOpVariable,
                            {ptr_type, var.spirv_var_id,
                             static_cast<uint32_t>(SpvStorageClassFunction)});
        }
      }

      for (const auto& inst : block.instructions) {
        if (!EmitInstruction(inst)) return false;
      }
    }

    AppendInstruction(&sections_->functions, SpvOpFunctionEnd, {});
    return true;
  }

  bool EmitInstruction(const ir::Instruction& inst) {
    switch (inst.kind) {
      case ir::InstKind::kVariable:
        return true;
      case ir::InstKind::kLoad:
        return EmitLoad(inst);
      case ir::InstKind::kStore:
        return EmitStore(inst);
      case ir::InstKind::kBinary:
        return EmitBinary(inst);
      case ir::InstKind::kBranch:
        return EmitBranch(inst);
      case ir::InstKind::kCondBranch:
        return EmitCondBranch(inst);
      case ir::InstKind::kReturn:
        return EmitReturn(inst);
      default:
        return false;
    }
  }

  /**
   * Finds a variable (local or global) by IR var id.
   * Returns the SPIR-V variable id, or 0 if not found.
   */
  uint32_t FindVariableSpirvId(uint32_t ir_var_id) {
    // Try local variables first
    auto local_it = FindLocalVar(ir_var_id);
    if (local_it != local_vars_.end()) {
      return local_it->spirv_var_id;
    }

    // Try global variables
    for (const auto& global : global_vars_) {
      if (global.ir_var_id == ir_var_id) {
        return global.spirv_var_id;
      }
    }

    return 0;
  }

  /**
   * Finds global variable info by IR var id.
   * Returns pointer to GlobalVarInfo, or nullptr if not found.
   */
  const GlobalVarInfo* FindGlobalVarInfo(uint32_t ir_var_id) const {
    for (const auto& global : global_vars_) {
      if (global.ir_var_id == ir_var_id) {
        return &global;
      }
    }
    return nullptr;
  }

  /**
   * Gets the pointer to use for load/store operations.
   * For wrapped buffers (uniform/storage with non-struct type),
   * emits OpAccessChain to access the struct member.
   * Returns the pointer id to use, or 0 on failure.
   */
  uint32_t GetAccessPointer(uint32_t ir_var_id, ir::TypeId inner_type,
                            uint32_t* out_ptr_id) {
    const GlobalVarInfo* global_info = FindGlobalVarInfo(ir_var_id);
    if (global_info == nullptr) {
      // Local variable - no access chain needed
      *out_ptr_id = FindVariableSpirvId(ir_var_id);
      return *out_ptr_id != 0 ? 1 : 0;  // Return non-zero to indicate success
    }

    if (!global_info->is_wrapped_buffer) {
      // Not a wrapped buffer - use variable directly
      *out_ptr_id = global_info->spirv_var_id;
      return 1;
    }

    // Wrapped buffer - need to access struct member
    // Get pointer type for the inner type
    uint32_t inner_ptr_type =
        type_emitter_->GetPointerType(inner_type, global_info->storage_class);
    if (inner_ptr_type == 0) return 0;

    // Emit OpAccessChain with index 0 to access the first (and only) member
    uint32_t access_id = ids_.Allocate();
    uint32_t zero_id = type_emitter_->EmitI32Constant(0);
    AppendInstruction(
        &sections_->functions, SpvOpAccessChain,
        {inner_ptr_type, access_id, global_info->spirv_var_id, zero_id});

    *out_ptr_id = access_id;
    return access_id;
  }

  /**
   * Emit explicit load instruction (new Value model).
   * kLoad in IR translates to SpvOpLoad in SPIR-V.
   */
  bool EmitLoad(const ir::Instruction& inst) {
    /**
     * kLoad instruction format:
     * - result_id: SSA id for the loaded value
     * - result_type: type of the loaded value
     * - operands[0]: variable value to load from
     */
    if (inst.operands.size() != 1) return false;
    const ir::Value& source = inst.operands[0];
    if (!source.IsVariable()) return false;

    uint32_t ir_var_id = source.GetVarId().value();

    // For wrapped buffers, get access pointer to struct member
    uint32_t ptr_id = 0;
    if (GetAccessPointer(ir_var_id, inst.result_type, &ptr_id) == 0) {
      return false;
    }

    uint32_t spirv_result_id = ids_.Allocate();
    AppendInstruction(
        &sections_->functions, SpvOpLoad,
        {GetSpirvTypeId(inst.result_type), spirv_result_id, ptr_id});

    /**
     * Map the IR SSA id to SPIR-V id.
     * This allows subsequent instructions to reference this loaded value.
     */
    value_map_[inst.result_id] = spirv_result_id;
    return true;
  }

  bool EmitStore(const ir::Instruction& inst) {
    if (inst.operands.size() != 2) return false;
    const ir::Value& target = inst.operands[0];
    const ir::Value& source = inst.operands[1];
    if (!target.IsVariable() || !source.IsValue()) return false;

    uint32_t ir_var_id = target.GetVarId().value();

    // For wrapped buffers, get access pointer to struct member
    uint32_t ptr_id = 0;
    if (GetAccessPointer(ir_var_id, source.type, &ptr_id) == 0) {
      return false;
    }

    uint32_t value_id = 0;
    if (!MaterializeValue(source, &value_id)) {
      return false;
    }

    AppendInstruction(&sections_->functions, SpvOpStore, {ptr_id, value_id});
    return true;
  }

  bool EmitBinary(const ir::Instruction& inst) {
    if (FindValue(inst.result_id) == values_.end()) return false;
    if (inst.operands.size() != 2) return false;

    uint32_t lhs_id = 0;
    if (!MaterializeValue(inst.operands[0], &lhs_id)) return false;
    uint32_t rhs_id = 0;
    if (!MaterializeValue(inst.operands[1], &rhs_id)) return false;

    SpvOp op = SpvOpNop;
    switch (inst.binary_op) {
      case ir::BinaryOpKind::kAdd:
        op = SpvOpFAdd;
        break;
      case ir::BinaryOpKind::kSubtract:
        op = SpvOpFSub;
        break;
    }

    uint32_t result_id = ids_.Allocate();
    AppendInstruction(
        &sections_->functions, op,
        {GetSpirvTypeId(inst.result_type), result_id, lhs_id, rhs_id});
    value_map_[inst.result_id] = result_id;
    return true;
  }

  bool MaterializeValue(const ir::Value& value, uint32_t* value_id) {
    if (value_id == nullptr || !value.IsValue()) return false;

    if (value.IsSSA()) {
      auto value_map_it = value_map_.find(value.GetSSAId().value());
      if (value_map_it == value_map_.end()) return false;
      *value_id = value_map_it->second;
      return true;
    }

    if (value.IsConstant()) {
      uint32_t const_id = EmitConstant(value);
      if (const_id != 0) {
        *value_id = const_id;
        return true;
      }
    }

    return false;
  }

  uint32_t GetSpirvTypeId(ir::TypeId type_id) {
    return type_emitter_->EmitType(type_id);
  }

  /**
   * Emits a constant value to the types_consts_globals section.
   * Returns the SPIR-V id of the constant, or 0 on failure.
   */
  uint32_t EmitConstant(const ir::Value& value) {
    if (!value.IsConstant()) return 0;

    switch (value.const_kind) {
      case ir::InlineConstKind::kF32:
        return type_emitter_->EmitF32Constant(value.GetF32Unchecked());
      case ir::InlineConstKind::kI32:
        return type_emitter_->EmitI32Constant(value.GetI32Unchecked());
      case ir::InlineConstKind::kU32:
        return type_emitter_->EmitU32Constant(value.GetU32Unchecked());
      case ir::InlineConstKind::kBool:
        return type_emitter_->EmitBoolConstant(value.GetBoolUnchecked());
      case ir::InlineConstKind::kVec2F32:
      case ir::InlineConstKind::kVec3F32:
      case ir::InlineConstKind::kVec4F32:
        return EmitConstantComposite(value);
      default:
        return 0;
    }
  }

  /**
   * Emits a vector constant using OpConstantComposite.
   * This is suitable for global variable initializers.
   */
  uint32_t EmitConstantComposite(const ir::Value& value) {
    // Get the vector dimension from the type
    ir::TypeId vector_type_id = value.type;
    const ir::Type* vector_type =
        type_emitter_->GetTypeTable()->GetType(vector_type_id);
    if (vector_type == nullptr || vector_type->kind != ir::TypeKind::kVector) {
      return 0;
    }

    uint32_t component_count = vector_type->count;
    if (component_count < 2 || component_count > 4) {
      return 0;
    }

    ir::TypeId element_type_id = vector_type->element_type;
    const ir::Type* element_type =
        type_emitter_->GetTypeTable()->GetType(element_type_id);
    if (element_type == nullptr) return 0;

    // Get component values from the Value (stored as raw float array)
    std::array<float, 4> raw_values = {0.0f, 0.0f, 0.0f, 0.0f};
    switch (value.const_kind) {
      case ir::InlineConstKind::kVec2F32:
        raw_values = {value.GetVec4Unchecked()[0], value.GetVec4Unchecked()[1],
                      0.0f, 0.0f};
        break;
      case ir::InlineConstKind::kVec3F32:
        raw_values = {value.GetVec4Unchecked()[0], value.GetVec4Unchecked()[1],
                      value.GetVec4Unchecked()[2], 0.0f};
        break;
      case ir::InlineConstKind::kVec4F32:
        raw_values = value.GetVec4Unchecked();
        break;
      default:
        return 0;
    }

    // Emit component constants based on element type
    std::vector<uint32_t> const_ids;
    const_ids.reserve(component_count);
    for (uint32_t i = 0; i < component_count; ++i) {
      uint32_t component_id = 0;
      if (element_type->kind == ir::TypeKind::kF32) {
        component_id = type_emitter_->EmitF32Constant(raw_values[i]);
      } else if (element_type->kind == ir::TypeKind::kI32) {
        int32_t iv = 0;
        std::memcpy(&iv, &raw_values[i], sizeof(iv));
        component_id = type_emitter_->EmitI32Constant(iv);
      } else if (element_type->kind == ir::TypeKind::kU32) {
        uint32_t uv = 0;
        std::memcpy(&uv, &raw_values[i], sizeof(uv));
        component_id = type_emitter_->EmitU32Constant(uv);
      } else if (element_type->kind == ir::TypeKind::kBool) {
        bool bv = false;
        std::memcpy(&bv, &raw_values[i], sizeof(bv));
        component_id = type_emitter_->EmitBoolConstant(bv);
      } else {
        return 0;
      }
      if (component_id == 0) return 0;
      const_ids.push_back(component_id);
    }

    // Emit OpConstantComposite to types_consts_globals
    uint32_t spirv_vector_type = type_emitter_->EmitType(vector_type_id);
    if (spirv_vector_type == 0) return 0;

    uint32_t result_id = ids_.Allocate();
    std::vector<uint32_t> operands;
    operands.push_back(spirv_vector_type);
    operands.push_back(result_id);
    operands.insert(operands.end(), const_ids.begin(), const_ids.end());

    AppendInstruction(&sections_->types_consts_globals, SpvOpConstantComposite,
                      operands);
    return result_id;
  }

  /**
   * Emits a wrapped struct constant for buffer initializers.
   * The inner value is wrapped in a single-member struct.
   */
  uint32_t EmitWrappedConstant(const ir::Value& inner_value,
                               ir::TypeId struct_type_id) {
    // Emit the inner constant
    uint32_t inner_id = EmitConstant(inner_value);
    if (inner_id == 0) return 0;

    // Emit the struct type
    uint32_t spirv_struct_type = type_emitter_->EmitType(struct_type_id);
    if (spirv_struct_type == 0) return 0;

    // Emit OpConstantComposite for the struct
    uint32_t result_id = ids_.Allocate();
    AppendInstruction(&sections_->types_consts_globals, SpvOpConstantComposite,
                      {spirv_struct_type, result_id, inner_id});
    return result_id;
  }

  bool EmitBranch(const ir::Instruction& inst) {
    if (inst.target_block == ir::kInvalidBlockId) return false;
    AppendInstruction(&sections_->functions, SpvOpBranch,
                      {GetOrCreateBlockLabel(inst.target_block)});
    return true;
  }

  bool EmitCondBranch(const ir::Instruction& inst) {
    if (inst.operands.size() != 1 || !inst.operands[0].IsValue()) return false;

    uint32_t cond_id = 0;
    if (!MaterializeValue(inst.operands[0], &cond_id)) {
      return false;
    }

    AppendInstruction(&sections_->functions, SpvOpSelectionMerge,
                      {GetOrCreateBlockLabel(inst.merge_block),
                       static_cast<uint32_t>(SpvSelectionControlMaskNone)});
    AppendInstruction(&sections_->functions, SpvOpBranchConditional,
                      {cond_id, GetOrCreateBlockLabel(inst.true_block),
                       GetOrCreateBlockLabel(inst.false_block)});
    return true;
  }

  bool EmitReturn(const ir::Instruction& inst) {
    if (inst.operands.empty()) {
      AppendInstruction(&sections_->functions, SpvOpReturn, {});
      return true;
    }

    if (inst.operands.size() != 1 || !inst.operands[0].IsValue()) return false;

    uint32_t value_to_store = 0;
    if (!MaterializeValue(inst.operands[0], &value_to_store)) {
      return false;
    }

    // Store to each output variable
    // For now, we only support single output (simple scalar/vector returns)
    // Future: struct returns will need member extraction and multiple stores
    for (const auto& output : output_vars_) {
      AppendInstruction(&sections_->functions, SpvOpStore,
                        {output.spirv_var_id, value_to_store});
    }
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

  std::vector<ValueInfo>::iterator FindValue(uint32_t ir_value_id) {
    for (auto it = values_.begin(); it != values_.end(); ++it) {
      if (it->ir_value_id == ir_value_id) return it;
    }
    return values_.end();
  }

  // Information about an output variable (mirrors ir::OutputVariable with
  // SPIR-V ids)
  struct OutputVarInfo {
    std::string name;
    ir::TypeId ir_type = ir::kInvalidTypeId;
    ir::OutputDecorationKind decoration_kind = ir::OutputDecorationKind::kNone;
    uint32_t decoration_value = 0;
    uint32_t spirv_var_id = 0;  // SPIR-V id for the OpVariable

    ir::BuiltinType GetBuiltin() const {
      if (decoration_kind == ir::OutputDecorationKind::kBuiltin) {
        return static_cast<ir::BuiltinType>(decoration_value);
      }
      return ir::BuiltinType::kNone;
    }

    uint32_t GetLocation() const {
      if (decoration_kind == ir::OutputDecorationKind::kLocation) {
        return decoration_value;
      }
      return 0;
    }
  };

 private:
  const ir::Module& module_;
  const ir::Function& entry_;
  SpvExecutionModel execution_model_;

  IdAllocator ids_;
  SectionBuffers* sections_ = nullptr;
  std::unique_ptr<TypeEmitter> type_emitter_;

  uint32_t function_id_ = 0;
  uint32_t function_type_id_ = 0;
  std::unordered_map<ir::BlockId, uint32_t> block_label_map_;

  // Output variables for entry point interface
  std::vector<OutputVarInfo> output_vars_;

  std::vector<LocalVarInfo> local_vars_;
  std::vector<GlobalVarInfo> global_vars_;
  std::vector<ValueInfo> values_;
  std::unordered_map<uint32_t, uint32_t> value_map_;

  uint32_t GetOrCreateBlockLabel(ir::BlockId block_id) {
    auto it = block_label_map_.find(block_id);
    if (it != block_label_map_.end()) {
      return it->second;
    }
    uint32_t label_id = ids_.Allocate();
    block_label_map_[block_id] = label_id;
    return label_id;
  }
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
