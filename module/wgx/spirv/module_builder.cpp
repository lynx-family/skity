// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>

#include "spirv/emitter_internal.h"

namespace wgx {
namespace spirv {

namespace {

bool ResolveBinaryOpcode(ir::BinaryOpKind op_kind, ir::TypeId operand_type,
                         ir::TypeId result_type, ir::TypeTable* type_table,
                         SpvOp* out_op) {
  if (type_table == nullptr || out_op == nullptr) {
    return false;
  }

  const bool is_bool = operand_type == type_table->GetBoolType();
  const bool is_int = type_table->IsIntegerType(operand_type);
  const bool is_float = type_table->IsFloatType(operand_type);
  const bool is_float_vector =
      type_table->IsVectorType(operand_type) &&
      type_table->GetComponentType(operand_type) == type_table->GetF32Type();
  const bool is_signed = operand_type == type_table->GetI32Type();
  const bool is_unsigned = operand_type == type_table->GetU32Type();

  switch (op_kind) {
    case ir::BinaryOpKind::kAdd:
      if (result_type != operand_type) {
        return false;
      }
      if (is_float || is_float_vector) {
        *out_op = SpvOpFAdd;
        return true;
      }
      if (is_int) {
        *out_op = SpvOpIAdd;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kSubtract:
      if (result_type != operand_type) {
        return false;
      }
      if (is_float || is_float_vector) {
        *out_op = SpvOpFSub;
        return true;
      }
      if (is_int) {
        *out_op = SpvOpISub;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kEqual:
      if (result_type != type_table->GetBoolType()) {
        return false;
      }
      if (is_bool) {
        *out_op = SpvOpLogicalEqual;
        return true;
      }
      if (is_int) {
        *out_op = SpvOpIEqual;
        return true;
      }
      if (is_float) {
        *out_op = SpvOpFOrdEqual;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kNotEqual:
      if (result_type != type_table->GetBoolType()) {
        return false;
      }
      if (is_bool) {
        *out_op = SpvOpLogicalNotEqual;
        return true;
      }
      if (is_int) {
        *out_op = SpvOpINotEqual;
        return true;
      }
      if (is_float) {
        *out_op = SpvOpFOrdNotEqual;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kLessThan:
      if (result_type != type_table->GetBoolType()) {
        return false;
      }
      if (is_signed) {
        *out_op = SpvOpSLessThan;
        return true;
      }
      if (is_unsigned) {
        *out_op = SpvOpULessThan;
        return true;
      }
      if (is_float) {
        *out_op = SpvOpFOrdLessThan;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kGreaterThan:
      if (result_type != type_table->GetBoolType()) {
        return false;
      }
      if (is_signed) {
        *out_op = SpvOpSGreaterThan;
        return true;
      }
      if (is_unsigned) {
        *out_op = SpvOpUGreaterThan;
        return true;
      }
      if (is_float) {
        *out_op = SpvOpFOrdGreaterThan;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kLessThanEqual:
      if (result_type != type_table->GetBoolType()) {
        return false;
      }
      if (is_signed) {
        *out_op = SpvOpSLessThanEqual;
        return true;
      }
      if (is_unsigned) {
        *out_op = SpvOpULessThanEqual;
        return true;
      }
      if (is_float) {
        *out_op = SpvOpFOrdLessThanEqual;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kGreaterThanEqual:
      if (result_type != type_table->GetBoolType()) {
        return false;
      }
      if (is_signed) {
        *out_op = SpvOpSGreaterThanEqual;
        return true;
      }
      if (is_unsigned) {
        *out_op = SpvOpUGreaterThanEqual;
        return true;
      }
      if (is_float) {
        *out_op = SpvOpFOrdGreaterThanEqual;
        return true;
      }
      return false;
  }

  return false;
}

}  // namespace

ModuleBuilder::ModuleBuilder(const ir::Module& module,
                             const ir::Function& entry,
                             SpvExecutionModel execution_model)
    : module_(module), entry_(entry), execution_model_(execution_model) {}

bool ModuleBuilder::Build(SectionBuffers* sections,
                          std::vector<uint32_t>* output_words) {
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

bool ModuleBuilder::AnalyzeEntryBlock() {
  const ir::Block* entry_block = entry_.GetBlock(entry_.entry_block_id);
  if (entry_block == nullptr || entry_.blocks.empty()) return false;

  for (const auto& ir_output : entry_.output_vars) {
    OutputVarInfo info;
    info.name = ir_output.name;
    info.ir_type = ir_output.type;
    info.decoration_kind = ir_output.decoration_kind;
    info.decoration_value = ir_output.decoration_value;
    output_vars_.push_back(std::move(info));
  }

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

void ModuleBuilder::CollectGlobalVarReferences(
    const std::vector<ir::Block>& blocks) {
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

bool ModuleBuilder::AllocateCoreIds() {
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

void ModuleBuilder::WriteCapabilityMemoryModel() {
  AppendInstruction(&sections_->capabilities, SpvOpCapability,
                    {static_cast<uint32_t>(SpvCapabilityShader)});
  AppendInstruction(&sections_->memory_model, SpvOpMemoryModel,
                    {static_cast<uint32_t>(SpvAddressingModelLogical),
                     static_cast<uint32_t>(SpvMemoryModelGLSL450)});
}

void ModuleBuilder::WriteEntryPointSection() {
  std::vector<uint32_t> interface_ids;
  for (const auto& output : output_vars_) {
    interface_ids.push_back(output.spirv_var_id);
  }
  AppendEntryPoint(&sections_->entry_points, execution_model_, function_id_,
                   module_.entry_point, interface_ids);
}

void ModuleBuilder::WriteExecutionModeSection() {
  if (module_.stage != ir::PipelineStage::kFragment) return;
  AppendInstruction(
      &sections_->execution_modes, SpvOpExecutionMode,
      {function_id_, static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
}

void ModuleBuilder::WriteDebugSection() {
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

namespace {
SpvBuiltIn ConvertBuiltin(ir::BuiltinType builtin) {
  switch (builtin) {
    case ir::BuiltinType::kPosition:
      return SpvBuiltInPosition;
    case ir::BuiltinType::kNone:
    default:
      return SpvBuiltInMax;
  }
}
}  // namespace

void ModuleBuilder::WriteAnnotationSection() {
  for (const auto& output : output_vars_) {
    if (output.decoration_kind == ir::OutputDecorationKind::kBuiltin) {
      SpvBuiltIn spirv_builtin = ConvertBuiltin(output.GetBuiltin());
      AppendInstruction(
          &sections_->annotations, SpvOpDecorate,
          {output.spirv_var_id, static_cast<uint32_t>(SpvDecorationBuiltIn),
           static_cast<uint32_t>(spirv_builtin)});
    } else if (output.decoration_kind == ir::OutputDecorationKind::kLocation) {
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
    if (global.is_wrapped_buffer) {
      uint32_t struct_type_id = type_emitter_->EmitType(global.var_type);
      if (struct_type_id != 0) {
        AppendInstruction(
            &sections_->annotations, SpvOpDecorate,
            {struct_type_id, static_cast<uint32_t>(SpvDecorationBlock)});
      }
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

bool ModuleBuilder::WriteTypeConstGlobalSection() {
  uint32_t void_type =
      type_emitter_->EmitType(type_emitter_->GetTypeTable()->GetVoidType());
  if (void_type == 0) return false;

  function_type_id_ = type_emitter_->GetFunctionTypeVoid();
  if (function_type_id_ == 0) return false;

  for (auto& output : output_vars_) {
    uint32_t spirv_type = type_emitter_->EmitType(output.ir_type);
    if (spirv_type == 0) return false;

    uint32_t output_ptr_type =
        type_emitter_->GetPointerType(output.ir_type, SpvStorageClassOutput);
    if (output_ptr_type == 0) return false;

    AppendInstruction(&sections_->types_consts_globals, SpvOpVariable,
                      {output_ptr_type, output.spirv_var_id,
                       static_cast<uint32_t>(SpvStorageClassOutput)});
  }

  for (auto& global : global_vars_) {
    uint32_t spirv_type = type_emitter_->EmitType(global.var_type);
    if (spirv_type == 0) return false;

    uint32_t global_ptr_type =
        type_emitter_->GetPointerType(global.var_type, global.storage_class);
    if (global_ptr_type == 0) return false;

    std::vector<uint32_t> operands = {
        global_ptr_type, global.spirv_var_id,
        static_cast<uint32_t>(global.storage_class)};

    if (global.initializer.has_value()) {
      uint32_t init_id = 0;
      if (global.is_wrapped_buffer) {
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

bool ModuleBuilder::WriteFunctionSection() {
  uint32_t void_type =
      type_emitter_->EmitType(type_emitter_->GetTypeTable()->GetVoidType());

  AppendInstruction(
      &sections_->functions, SpvOpFunction,
      {void_type, function_id_,
       static_cast<uint32_t>(SpvFunctionControlMaskNone), function_type_id_});

  for (const auto& block : entry_.blocks) {
    current_block_ = &block;
    uint32_t block_label_id = GetOrCreateBlockLabel(block.id);
    AppendInstruction(&sections_->functions, SpvOpLabel, {block_label_id});

    if (block.IsLoopHeader()) {
      AppendInstruction(&sections_->functions, SpvOpLoopMerge,
                        {GetOrCreateBlockLabel(block.loop_merge_block),
                         GetOrCreateBlockLabel(block.loop_continue_block),
                         static_cast<uint32_t>(SpvLoopControlMaskNone)});
    }

    if (block.id == entry_.entry_block_id) {
      for (auto& var : local_vars_) {
        uint32_t ptr_type = type_emitter_->GetPointerType(
            var.var_type, SpvStorageClassFunction);
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

  current_block_ = nullptr;

  AppendInstruction(&sections_->functions, SpvOpFunctionEnd, {});
  return true;
}

bool ModuleBuilder::EmitInstruction(const ir::Instruction& inst) {
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

uint32_t ModuleBuilder::FindVariableSpirvId(uint32_t ir_var_id) {
  auto local_it = FindLocalVar(ir_var_id);
  if (local_it != local_vars_.end()) {
    return local_it->spirv_var_id;
  }

  for (const auto& global : global_vars_) {
    if (global.ir_var_id == ir_var_id) {
      return global.spirv_var_id;
    }
  }

  return 0;
}

const GlobalVarInfo* ModuleBuilder::FindGlobalVarInfo(
    uint32_t ir_var_id) const {
  for (const auto& global : global_vars_) {
    if (global.ir_var_id == ir_var_id) {
      return &global;
    }
  }
  return nullptr;
}

uint32_t ModuleBuilder::GetAccessPointer(uint32_t ir_var_id,
                                         ir::TypeId inner_type,
                                         uint32_t* out_ptr_id) {
  const GlobalVarInfo* global_info = FindGlobalVarInfo(ir_var_id);
  if (global_info == nullptr) {
    *out_ptr_id = FindVariableSpirvId(ir_var_id);
    return *out_ptr_id != 0 ? 1 : 0;
  }

  if (!global_info->is_wrapped_buffer) {
    *out_ptr_id = global_info->spirv_var_id;
    return 1;
  }

  uint32_t inner_ptr_type =
      type_emitter_->GetPointerType(inner_type, global_info->storage_class);
  if (inner_ptr_type == 0) return 0;

  uint32_t access_id = ids_.Allocate();
  uint32_t zero_id = type_emitter_->EmitI32Constant(0);
  AppendInstruction(
      &sections_->functions, SpvOpAccessChain,
      {inner_ptr_type, access_id, global_info->spirv_var_id, zero_id});

  *out_ptr_id = access_id;
  return access_id;
}

bool ModuleBuilder::EmitLoad(const ir::Instruction& inst) {
  if (inst.operands.size() != 1) return false;
  const ir::Value& source = inst.operands[0];
  if (!source.IsVariable()) return false;

  uint32_t ir_var_id = source.GetVarId().value();
  uint32_t ptr_id = 0;
  if (GetAccessPointer(ir_var_id, inst.result_type, &ptr_id) == 0) {
    return false;
  }

  uint32_t spirv_result_id = ids_.Allocate();
  AppendInstruction(
      &sections_->functions, SpvOpLoad,
      {GetSpirvTypeId(inst.result_type), spirv_result_id, ptr_id});
  value_map_[inst.result_id] = spirv_result_id;
  return true;
}

bool ModuleBuilder::EmitStore(const ir::Instruction& inst) {
  if (inst.operands.size() != 2) return false;
  const ir::Value& target = inst.operands[0];
  const ir::Value& source = inst.operands[1];
  if (!target.IsVariable() || !source.IsValue()) return false;

  uint32_t ir_var_id = target.GetVarId().value();
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

bool ModuleBuilder::EmitBinary(const ir::Instruction& inst) {
  if (FindValue(inst.result_id) == values_.end()) return false;
  if (inst.operands.size() != 2) return false;

  uint32_t lhs_id = 0;
  if (!MaterializeValue(inst.operands[0], &lhs_id)) return false;
  uint32_t rhs_id = 0;
  if (!MaterializeValue(inst.operands[1], &rhs_id)) return false;

  SpvOp op = SpvOpNop;
  if (!ResolveBinaryOpcode(inst.binary_op, inst.operands[0].type,
                           inst.result_type, type_emitter_->GetTypeTable(),
                           &op)) {
    return false;
  }

  uint32_t result_id = ids_.Allocate();
  AppendInstruction(
      &sections_->functions, op,
      {GetSpirvTypeId(inst.result_type), result_id, lhs_id, rhs_id});
  value_map_[inst.result_id] = result_id;
  return true;
}

bool ModuleBuilder::MaterializeValue(const ir::Value& value,
                                     uint32_t* value_id) {
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

uint32_t ModuleBuilder::GetSpirvTypeId(ir::TypeId type_id) {
  return type_emitter_->EmitType(type_id);
}

uint32_t ModuleBuilder::EmitConstant(const ir::Value& value) {
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

uint32_t ModuleBuilder::EmitConstantComposite(const ir::Value& value) {
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

uint32_t ModuleBuilder::EmitWrappedConstant(const ir::Value& inner_value,
                                            ir::TypeId struct_type_id) {
  uint32_t inner_id = EmitConstant(inner_value);
  if (inner_id == 0) return 0;

  uint32_t spirv_struct_type = type_emitter_->EmitType(struct_type_id);
  if (spirv_struct_type == 0) return 0;

  uint32_t result_id = ids_.Allocate();
  AppendInstruction(&sections_->types_consts_globals, SpvOpConstantComposite,
                    {spirv_struct_type, result_id, inner_id});
  return result_id;
}

bool ModuleBuilder::EmitBranch(const ir::Instruction& inst) {
  if (inst.target_block == ir::kInvalidBlockId) return false;
  AppendInstruction(&sections_->functions, SpvOpBranch,
                    {GetOrCreateBlockLabel(inst.target_block)});
  return true;
}

bool ModuleBuilder::EmitCondBranch(const ir::Instruction& inst) {
  if (inst.operands.size() != 1 || !inst.operands[0].IsValue()) return false;

  uint32_t cond_id = 0;
  if (!MaterializeValue(inst.operands[0], &cond_id)) {
    return false;
  }

  if (current_block_ == nullptr) {
    return false;
  }

  if (inst.merge_block != ir::kInvalidBlockId &&
      !current_block_->IsLoopHeader()) {
    AppendInstruction(&sections_->functions, SpvOpSelectionMerge,
                      {GetOrCreateBlockLabel(inst.merge_block),
                       static_cast<uint32_t>(SpvSelectionControlMaskNone)});
  }
  AppendInstruction(&sections_->functions, SpvOpBranchConditional,
                    {cond_id, GetOrCreateBlockLabel(inst.true_block),
                     GetOrCreateBlockLabel(inst.false_block)});
  return true;
}

bool ModuleBuilder::EmitReturn(const ir::Instruction& inst) {
  if (inst.operands.empty()) {
    AppendInstruction(&sections_->functions, SpvOpReturn, {});
    return true;
  }

  if (inst.operands.size() != 1 || !inst.operands[0].IsValue()) return false;

  uint32_t value_to_store = 0;
  if (!MaterializeValue(inst.operands[0], &value_to_store)) {
    return false;
  }

  for (const auto& output : output_vars_) {
    AppendInstruction(&sections_->functions, SpvOpStore,
                      {output.spirv_var_id, value_to_store});
  }
  AppendInstruction(&sections_->functions, SpvOpReturn, {});
  return true;
}

void ModuleBuilder::AssembleModule(std::vector<uint32_t>* output_words) {
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

std::vector<LocalVarInfo>::iterator ModuleBuilder::FindLocalVar(
    uint32_t ir_var_id) {
  for (auto it = local_vars_.begin(); it != local_vars_.end(); ++it) {
    if (it->ir_var_id == ir_var_id) return it;
  }
  return local_vars_.end();
}

std::vector<ValueInfo>::iterator ModuleBuilder::FindValue(
    uint32_t ir_value_id) {
  for (auto it = values_.begin(); it != values_.end(); ++it) {
    if (it->ir_value_id == ir_value_id) return it;
  }
  return values_.end();
}

ir::BuiltinType ModuleBuilder::OutputVarInfo::GetBuiltin() const {
  if (decoration_kind == ir::OutputDecorationKind::kBuiltin) {
    return static_cast<ir::BuiltinType>(decoration_value);
  }
  return ir::BuiltinType::kNone;
}

uint32_t ModuleBuilder::OutputVarInfo::GetLocation() const {
  if (decoration_kind == ir::OutputDecorationKind::kLocation) {
    return decoration_value;
  }
  return 0;
}

uint32_t ModuleBuilder::GetOrCreateBlockLabel(ir::BlockId block_id) {
  auto it = block_label_map_.find(block_id);
  if (it != block_label_map_.end()) {
    return it->second;
  }
  uint32_t label_id = ids_.Allocate();
  block_label_map_[block_id] = label_id;
  return label_id;
}

}  // namespace spirv
}  // namespace wgx
