// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <unordered_set>

#include "spirv/emitter_internal.h"

namespace wgx {
namespace spirv {

namespace {

bool IsVectorScalarBinary(const ir::Instruction& inst,
                          ir::TypeTable* type_table) {
  if (type_table == nullptr || inst.operands.size() != 2u ||
      (inst.binary_op != ir::BinaryOpKind::kMultiply &&
       inst.binary_op != ir::BinaryOpKind::kDivide)) {
    return false;
  }

  const ir::TypeId lhs_type = inst.operands[0].type;
  const ir::TypeId rhs_type = inst.operands[1].type;
  if (lhs_type == rhs_type || !type_table->IsVectorType(inst.result_type)) {
    return false;
  }

  const ir::TypeId result_component =
      type_table->GetComponentType(inst.result_type);
  if (lhs_type == inst.result_type && type_table->IsScalarType(rhs_type) &&
      result_component == rhs_type) {
    return true;
  }

  return inst.binary_op == ir::BinaryOpKind::kMultiply &&
         rhs_type == inst.result_type && type_table->IsScalarType(lhs_type) &&
         result_component == lhs_type;
}

bool IsMatrixVectorMultiply(const ir::Instruction& inst,
                            ir::TypeTable* type_table) {
  if (type_table == nullptr || inst.operands.size() != 2u ||
      inst.binary_op != ir::BinaryOpKind::kMultiply) {
    return false;
  }

  const ir::TypeId lhs_type = inst.operands[0].type;
  const ir::TypeId rhs_type = inst.operands[1].type;
  if (!type_table->IsMatrixType(lhs_type) ||
      !type_table->IsVectorType(rhs_type) ||
      !type_table->IsVectorType(inst.result_type)) {
    return false;
  }

  const ir::Type* matrix_type = type_table->GetType(lhs_type);
  if (matrix_type == nullptr ||
      matrix_type->element_type != type_table->GetF32Type()) {
    return false;
  }

  return type_table->GetComponentType(rhs_type) == type_table->GetF32Type() &&
         type_table->GetComponentType(inst.result_type) ==
             type_table->GetF32Type() &&
         matrix_type->count2 == type_table->GetVectorComponentCount(rhs_type) &&
         matrix_type->count ==
             type_table->GetVectorComponentCount(inst.result_type);
}

bool IsMatrixMatrixMultiply(const ir::Instruction& inst,
                            ir::TypeTable* type_table) {
  if (type_table == nullptr || inst.operands.size() != 2u ||
      inst.binary_op != ir::BinaryOpKind::kMultiply) {
    return false;
  }

  const ir::TypeId lhs_type = inst.operands[0].type;
  const ir::TypeId rhs_type = inst.operands[1].type;
  if (!type_table->IsMatrixType(lhs_type) ||
      !type_table->IsMatrixType(rhs_type) ||
      !type_table->IsMatrixType(inst.result_type)) {
    return false;
  }

  const ir::Type* lhs_matrix_type = type_table->GetType(lhs_type);
  const ir::Type* rhs_matrix_type = type_table->GetType(rhs_type);
  const ir::Type* result_matrix_type = type_table->GetType(inst.result_type);
  if (lhs_matrix_type == nullptr || rhs_matrix_type == nullptr ||
      result_matrix_type == nullptr) {
    return false;
  }

  return lhs_matrix_type->element_type == type_table->GetF32Type() &&
         rhs_matrix_type->element_type == type_table->GetF32Type() &&
         result_matrix_type->element_type == type_table->GetF32Type() &&
         lhs_matrix_type->count2 == rhs_matrix_type->count &&
         result_matrix_type->count == lhs_matrix_type->count &&
         result_matrix_type->count2 == rhs_matrix_type->count2;
}

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
    case ir::BinaryOpKind::kMultiply:
      if (result_type != operand_type) {
        return false;
      }
      if (is_float || is_float_vector) {
        *out_op = SpvOpFMul;
        return true;
      }
      if (is_int) {
        *out_op = SpvOpIMul;
        return true;
      }
      return false;
    case ir::BinaryOpKind::kDivide:
      if (result_type != operand_type) {
        return false;
      }
      if (is_float || is_float_vector) {
        *out_op = SpvOpFDiv;
        return true;
      }
      if (is_signed) {
        *out_op = SpvOpSDiv;
        return true;
      }
      if (is_unsigned) {
        *out_op = SpvOpUDiv;
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

bool ResolveCastOpcode(ir::TypeId source_type, ir::TypeId result_type,
                       ir::TypeTable* type_table, SpvOp* out_op) {
  if (type_table == nullptr || out_op == nullptr) {
    return false;
  }
  if (source_type == result_type) {
    return false;
  }

  const bool source_is_float = source_type == type_table->GetF32Type();
  const bool source_is_i32 = source_type == type_table->GetI32Type();
  const bool source_is_u32 = source_type == type_table->GetU32Type();
  const bool result_is_float = result_type == type_table->GetF32Type();
  const bool result_is_i32 = result_type == type_table->GetI32Type();
  const bool result_is_u32 = result_type == type_table->GetU32Type();

  if (source_is_i32 && result_is_float) {
    *out_op = SpvOpConvertSToF;
    return true;
  }
  if (source_is_u32 && result_is_float) {
    *out_op = SpvOpConvertUToF;
    return true;
  }
  if (source_is_float && result_is_i32) {
    *out_op = SpvOpConvertFToS;
    return true;
  }
  if (source_is_float && result_is_u32) {
    *out_op = SpvOpConvertFToU;
    return true;
  }
  if (source_is_u32 && result_is_i32) {
    *out_op = SpvOpSConvert;
    return true;
  }
  if (source_is_i32 && result_is_u32) {
    *out_op = SpvOpUConvert;
    return true;
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

  input_vars_.clear();
  for (const auto& ir_input : entry_.input_vars) {
    InputVarInfo info;
    info.target_var_id = ir_input.target_var_id;
    info.member_index = ir_input.member_index;
    info.name = ir_input.name;
    info.ir_type = ir_input.type;
    info.decoration_kind = ir_input.decoration_kind;
    info.decoration_value = ir_input.decoration_value;
    input_vars_.push_back(std::move(info));
  }

  output_vars_.clear();
  for (const auto& ir_output : entry_.output_vars) {
    OutputVarInfo info;
    info.name = ir_output.name;
    info.ir_type = ir_output.type;
    info.member_index = ir_output.member_index;
    info.decoration_kind = ir_output.decoration_kind;
    info.decoration_value = ir_output.decoration_value;
    output_vars_.push_back(std::move(info));
  }

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

bool ModuleBuilder::AnalyzeFunction(const ir::Function& function) {
  function_params_.clear();
  local_vars_.clear();
  values_.clear();
  value_map_.clear();
  block_label_map_.clear();

  const ir::Block* entry_block = function.GetBlock(function.entry_block_id);
  if (entry_block == nullptr || function.blocks.empty()) return false;

  for (const auto& param : function.parameters) {
    FunctionParamInfo info;
    info.ir_var_id = param.var_id;
    info.var_type = param.type;
    info.name = param.name;
    info.spirv_local_id = ids_.Allocate();
    if (function.name != module_.entry_point) {
      info.spirv_param_id = ids_.Allocate();
    }
    function_params_.push_back(std::move(info));
  }

  for (const auto& block : function.blocks) {
    block_label_map_[block.id] = ids_.Allocate();
    for (const auto& inst : block.instructions) {
      if (inst.kind == ir::InstKind::kVariable) {
        LocalVarInfo info;
        info.ir_var_id = inst.var_id;
        info.var_type = inst.result_type;
        info.spirv_var_id = ids_.Allocate();
        local_vars_.push_back(std::move(info));
      } else if (inst.kind == ir::InstKind::kAccess ||
                 inst.kind == ir::InstKind::kExtract ||
                 inst.kind == ir::InstKind::kBinary ||
                 inst.kind == ir::InstKind::kCast ||
                 inst.kind == ir::InstKind::kLoad ||
                 inst.kind == ir::InstKind::kConstruct ||
                 inst.kind == ir::InstKind::kBuiltinCall ||
                 (inst.kind == ir::InstKind::kCall && inst.result_id != 0)) {
        ValueInfo info;
        info.ir_value_id = inst.result_id;
        info.value_type = inst.result_type;
        values_.push_back(std::move(info));
      }
    }
  }
  return true;
}

void ModuleBuilder::CollectGlobalVarReferences(const ir::Function& function) {
  std::unordered_set<uint32_t> local_var_ids;
  for (const auto& param : function.parameters) {
    local_var_ids.insert(param.var_id);
  }
  for (const auto& block : function.blocks) {
    for (const auto& inst : block.instructions) {
      if (inst.kind == ir::InstKind::kVariable) {
        local_var_ids.insert(inst.var_id);
      }
    }
  }

  for (const auto& block : function.blocks) {
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
  function_id_map_.clear();
  function_type_id_map_.clear();

  for (const auto& function : module_.functions) {
    function_id_map_[function.name] = ids_.Allocate();

    std::vector<ir::TypeId> param_types;
    if (function.name != module_.entry_point) {
      param_types.reserve(function.parameters.size());
      for (const auto& param : function.parameters) {
        param_types.push_back(param.type);
      }
    }

    const ir::TypeId return_type = GetSpirvFunctionReturnType(function);
    const uint32_t function_type_id =
        type_emitter_->GetFunctionType(return_type, param_types);
    if (function_type_id == 0) {
      return false;
    }
    function_type_id_map_[function.name] = function_type_id;
  }

  function_id_ = GetFunctionId(module_.entry_point);
  for (auto& input : input_vars_) {
    input.spirv_var_id = ids_.Allocate();
  }
  for (auto& output : output_vars_) {
    output.spirv_var_id = ids_.Allocate();
  }

  global_vars_.clear();
  for (const auto& function : module_.functions) {
    CollectGlobalVarReferences(function);
  }
  for (auto& global : global_vars_) {
    global.spirv_var_id = ids_.Allocate();
  }
  return function_id_ != 0;
}

void ModuleBuilder::WriteCapabilityMemoryModel() {
  AppendInstruction(&sections_->capabilities, SpvOpCapability,
                    {static_cast<uint32_t>(SpvCapabilityShader)});
  AppendInstruction(&sections_->capabilities, SpvOpCapability,
                    {static_cast<uint32_t>(SpvCapabilityImageQuery)});
  AppendInstruction(&sections_->memory_model, SpvOpMemoryModel,
                    {static_cast<uint32_t>(SpvAddressingModelLogical),
                     static_cast<uint32_t>(SpvMemoryModelGLSL450)});
}

void ModuleBuilder::WriteEntryPointSection() {
  std::vector<uint32_t> interface_ids;
  for (const auto& input : input_vars_) {
    interface_ids.push_back(input.spirv_var_id);
  }
  for (const auto& output : output_vars_) {
    interface_ids.push_back(output.spirv_var_id);
  }
  AppendEntryPoint(&sections_->entry_points, execution_model_,
                   GetFunctionId(module_.entry_point), module_.entry_point,
                   interface_ids);
}

void ModuleBuilder::WriteExecutionModeSection() {
  if (module_.stage != ir::PipelineStage::kFragment) return;
  AppendInstruction(&sections_->execution_modes, SpvOpExecutionMode,
                    {GetFunctionId(module_.entry_point),
                     static_cast<uint32_t>(SpvExecutionModeOriginUpperLeft)});
}

void ModuleBuilder::WriteDebugSection() {
  for (const auto& function : module_.functions) {
    AppendName(&sections_->debug, GetFunctionId(function.name), function.name);
  }
  for (const auto& input : input_vars_) {
    AppendName(&sections_->debug, input.spirv_var_id, input.name);
  }
  for (const auto& output : output_vars_) {
    AppendName(&sections_->debug, output.spirv_var_id, output.name);
  }
}

namespace {
SpvBuiltIn ConvertBuiltin(ir::BuiltinType builtin) {
  switch (builtin) {
    case ir::BuiltinType::kPosition:
      return SpvBuiltInPosition;
    case ir::BuiltinType::kVertexIndex:
      return SpvBuiltInVertexIndex;
    case ir::BuiltinType::kInstanceIndex:
      return SpvBuiltInInstanceIndex;
    case ir::BuiltinType::kNone:
    default:
      return SpvBuiltInMax;
  }
}
}  // namespace

void ModuleBuilder::WriteAnnotationSection() {
  for (const auto& input : input_vars_) {
    if (input.decoration_kind == ir::InterfaceDecorationKind::kBuiltin) {
      SpvBuiltIn spirv_builtin = ConvertBuiltin(input.GetBuiltin());
      AppendInstruction(
          &sections_->annotations, SpvOpDecorate,
          {input.spirv_var_id, static_cast<uint32_t>(SpvDecorationBuiltIn),
           static_cast<uint32_t>(spirv_builtin)});
    } else if (input.decoration_kind ==
               ir::InterfaceDecorationKind::kLocation) {
      AppendInstruction(
          &sections_->annotations, SpvOpDecorate,
          {input.spirv_var_id, static_cast<uint32_t>(SpvDecorationLocation),
           input.decoration_value});
    }
  }
  for (const auto& output : output_vars_) {
    if (output.decoration_kind == ir::InterfaceDecorationKind::kBuiltin) {
      SpvBuiltIn spirv_builtin = ConvertBuiltin(output.GetBuiltin());
      AppendInstruction(
          &sections_->annotations, SpvOpDecorate,
          {output.spirv_var_id, static_cast<uint32_t>(SpvDecorationBuiltIn),
           static_cast<uint32_t>(spirv_builtin)});
    } else if (output.decoration_kind ==
               ir::InterfaceDecorationKind::kLocation) {
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

  for (auto& input : input_vars_) {
    uint32_t spirv_type = type_emitter_->EmitType(input.ir_type);
    if (spirv_type == 0) return false;

    uint32_t input_ptr_type =
        type_emitter_->GetPointerType(input.ir_type, SpvStorageClassInput);
    if (input_ptr_type == 0) return false;

    AppendInstruction(&sections_->types_consts_globals, SpvOpVariable,
                      {input_ptr_type, input.spirv_var_id,
                       static_cast<uint32_t>(SpvStorageClassInput)});
  }

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
  for (const auto& function : module_.functions) {
    if (!AnalyzeFunction(function)) {
      return false;
    }

    function_id_ = GetFunctionId(function.name);
    function_type_id_ = GetFunctionTypeId(function.name);
    uint32_t return_type_id =
        type_emitter_->EmitType(GetSpirvFunctionReturnType(function));
    if (function_id_ == 0 || function_type_id_ == 0 || return_type_id == 0) {
      return false;
    }

    AppendInstruction(
        &sections_->functions, SpvOpFunction,
        {return_type_id, function_id_,
         static_cast<uint32_t>(SpvFunctionControlMaskNone), function_type_id_});

    if (function.name != module_.entry_point) {
      for (const auto& param : function_params_) {
        uint32_t param_type_id = type_emitter_->EmitType(param.var_type);
        if (param_type_id == 0) {
          return false;
        }
        AppendInstruction(&sections_->functions, SpvOpFunctionParameter,
                          {param_type_id, param.spirv_param_id});
      }
    }

    for (const auto& block : function.blocks) {
      current_block_ = &block;
      uint32_t block_label_id = GetOrCreateBlockLabel(block.id);
      AppendInstruction(&sections_->functions, SpvOpLabel, {block_label_id});

      if (block.IsLoopHeader()) {
        AppendInstruction(&sections_->functions, SpvOpLoopMerge,
                          {GetOrCreateBlockLabel(block.loop_merge_block),
                           GetOrCreateBlockLabel(block.loop_continue_block),
                           static_cast<uint32_t>(SpvLoopControlMaskNone)});
      }

      if (block.id == function.entry_block_id) {
        for (auto& param : function_params_) {
          uint32_t ptr_type = type_emitter_->GetPointerType(
              param.var_type, SpvStorageClassFunction);
          if (ptr_type == 0) return false;
          AppendInstruction(&sections_->functions, SpvOpVariable,
                            {ptr_type, param.spirv_local_id,
                             static_cast<uint32_t>(SpvStorageClassFunction)});
        }

        for (auto& var : local_vars_) {
          uint32_t ptr_type = type_emitter_->GetPointerType(
              var.var_type, SpvStorageClassFunction);
          if (ptr_type == 0) return false;
          AppendInstruction(&sections_->functions, SpvOpVariable,
                            {ptr_type, var.spirv_var_id,
                             static_cast<uint32_t>(SpvStorageClassFunction)});
        }

        if (function.name == module_.entry_point) {
          for (const auto& input : input_vars_) {
            uint32_t loaded_input_id = ids_.Allocate();
            AppendInstruction(&sections_->functions, SpvOpLoad,
                              {GetSpirvTypeId(input.ir_type), loaded_input_id,
                               input.spirv_var_id});

            uint32_t ptr_id = 0;
            if (input.member_index.has_value()) {
              const FunctionParamInfo* param_info =
                  FindFunctionParamInfo(input.target_var_id);
              if (param_info == nullptr) {
                return false;
              }

              uint32_t ptr_type = type_emitter_->GetPointerType(
                  input.ir_type, SpvStorageClassFunction);
              uint32_t member_index_id =
                  type_emitter_->EmitI32Constant(input.member_index.value());
              if (ptr_type == 0 || member_index_id == 0) {
                return false;
              }

              ptr_id = ids_.Allocate();
              AppendInstruction(&sections_->functions, SpvOpAccessChain,
                                {ptr_type, ptr_id, param_info->spirv_local_id,
                                 member_index_id});
            } else {
              ptr_id = FindVariableSpirvId(input.target_var_id);
            }
            if (ptr_id == 0) {
              return false;
            }
            AppendInstruction(&sections_->functions, SpvOpStore,
                              {ptr_id, loaded_input_id});
          }
        } else {
          for (auto& param : function_params_) {
            AppendInstruction(&sections_->functions, SpvOpStore,
                              {param.spirv_local_id, param.spirv_param_id});
          }
        }
      }

      for (const auto& inst : block.instructions) {
        if (!EmitInstruction(inst)) return false;
      }
    }

    current_block_ = nullptr;
    AppendInstruction(&sections_->functions, SpvOpFunctionEnd, {});
  }
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
    case ir::InstKind::kAccess:
      return EmitAccess(inst);
    case ir::InstKind::kExtract:
      return EmitExtract(inst);
    case ir::InstKind::kBinary:
      return EmitBinary(inst);
    case ir::InstKind::kCast:
      return EmitCast(inst);
    case ir::InstKind::kConstruct:
      return EmitConstruct(inst);
    case ir::InstKind::kCall:
      return EmitCall(inst);
    case ir::InstKind::kBuiltinCall:
      return EmitBuiltinCall(inst);
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
  for (const auto& param : function_params_) {
    if (param.ir_var_id == ir_var_id) {
      return param.spirv_local_id;
    }
  }

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

const FunctionParamInfo* ModuleBuilder::FindFunctionParamInfo(
    uint32_t ir_var_id) const {
  for (const auto& param : function_params_) {
    if (param.ir_var_id == ir_var_id) {
      return &param;
    }
  }
  return nullptr;
}

const ir::Function* ModuleBuilder::FindFunctionByName(
    std::string_view name) const {
  for (const auto& function : module_.functions) {
    if (function.name == name) {
      return &function;
    }
  }
  return nullptr;
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

uint32_t ModuleBuilder::GetAddressPointer(const ir::Value& address,
                                          ir::TypeId inner_type,
                                          uint32_t* out_ptr_id) {
  if (out_ptr_id == nullptr || !address.IsAddress()) {
    return 0;
  }

  if (address.IsPointerSSA()) {
    *out_ptr_id = value_map_[address.GetSSAId().value_or(0)];
    return *out_ptr_id != 0 ? 1 : 0;
  }

  uint32_t ir_var_id = address.GetVarId().value_or(0);
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
  if (!source.IsAddress()) return false;

  uint32_t ptr_id = 0;
  if (GetAddressPointer(source, inst.result_type, &ptr_id) == 0) {
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
  if (!target.IsAddress() || !source.IsValue()) return false;

  uint32_t ptr_id = 0;
  if (GetAddressPointer(target, source.type, &ptr_id) == 0) {
    return false;
  }

  uint32_t value_id = 0;
  if (!MaterializeValue(source, &value_id)) {
    return false;
  }

  AppendInstruction(&sections_->functions, SpvOpStore, {ptr_id, value_id});
  return true;
}

bool ModuleBuilder::EmitAccess(const ir::Instruction& inst) {
  if (inst.operands.size() != 1 || FindValue(inst.result_id) == values_.end()) {
    return false;
  }
  const ir::Value& base = inst.operands[0];
  if (!base.IsAddress()) {
    return false;
  }

  uint32_t base_ptr_id = 0;
  if (!MaterializeAddress(base, base.type, &base_ptr_id)) {
    return false;
  }

  uint32_t ptr_type =
      type_emitter_->GetPointerType(inst.result_type, SpvStorageClassFunction);
  if (ptr_type == 0) {
    const GlobalVarInfo* global_info =
        base.IsVariable() ? FindGlobalVarInfo(base.GetVarId().value_or(0))
                          : nullptr;
    if (global_info != nullptr) {
      ptr_type = type_emitter_->GetPointerType(inst.result_type,
                                               global_info->storage_class);
    }
  }
  if (ptr_type == 0) {
    return false;
  }

  uint32_t access_id = ids_.Allocate();
  uint32_t index_id = type_emitter_->EmitI32Constant(inst.access_index);
  AppendInstruction(&sections_->functions, SpvOpAccessChain,
                    {ptr_type, access_id, base_ptr_id, index_id});
  value_map_[inst.result_id] = access_id;
  return true;
}

bool ModuleBuilder::EmitExtract(const ir::Instruction& inst) {
  if (inst.operands.size() != 1 || FindValue(inst.result_id) == values_.end()) {
    return false;
  }
  const ir::Value& base = inst.operands[0];
  if (!base.IsValue()) {
    return false;
  }

  uint32_t base_id = 0;
  if (!MaterializeValue(base, &base_id)) {
    return false;
  }

  uint32_t result_id = ids_.Allocate();
  AppendInstruction(&sections_->functions, SpvOpCompositeExtract,
                    {GetSpirvTypeId(inst.result_type), result_id, base_id,
                     inst.access_index});
  value_map_[inst.result_id] = result_id;
  return true;
}

bool ModuleBuilder::EmitBinary(const ir::Instruction& inst) {
  if (FindValue(inst.result_id) == values_.end()) return false;
  if (inst.operands.size() != 2) return false;

  uint32_t lhs_id = 0;
  uint32_t rhs_id = 0;
  ir::TypeTable* type_table = type_emitter_->GetTypeTable();
  if (type_table == nullptr) {
    return false;
  }

  if (IsVectorScalarBinary(inst, type_table)) {
    if (!MaterializeBinaryOperand(inst.operands[0], inst.result_type,
                                  &lhs_id) ||
        !MaterializeBinaryOperand(inst.operands[1], inst.result_type,
                                  &rhs_id)) {
      return false;
    }
  } else {
    if (!MaterializeValue(inst.operands[0], &lhs_id) ||
        !MaterializeValue(inst.operands[1], &rhs_id)) {
      return false;
    }
  }

  if (IsMatrixVectorMultiply(inst, type_table)) {
    uint32_t result_id = ids_.Allocate();
    AppendInstruction(
        &sections_->functions, SpvOpMatrixTimesVector,
        {GetSpirvTypeId(inst.result_type), result_id, lhs_id, rhs_id});
    value_map_[inst.result_id] = result_id;
    return true;
  }

  if (IsMatrixMatrixMultiply(inst, type_table)) {
    uint32_t result_id = ids_.Allocate();
    AppendInstruction(
        &sections_->functions, SpvOpMatrixTimesMatrix,
        {GetSpirvTypeId(inst.result_type), result_id, lhs_id, rhs_id});
    value_map_[inst.result_id] = result_id;
    return true;
  }

  const ir::TypeId opcode_operand_type =
      inst.operands[0].type == inst.operands[1].type ? inst.operands[0].type
                                                     : inst.result_type;
  SpvOp op = SpvOpNop;
  if (!ResolveBinaryOpcode(inst.binary_op, opcode_operand_type,
                           inst.result_type, type_table, &op)) {
    return false;
  }

  uint32_t result_id = ids_.Allocate();
  AppendInstruction(
      &sections_->functions, op,
      {GetSpirvTypeId(inst.result_type), result_id, lhs_id, rhs_id});
  value_map_[inst.result_id] = result_id;
  return true;
}

bool ModuleBuilder::EmitCast(const ir::Instruction& inst) {
  if (FindValue(inst.result_id) == values_.end()) {
    return false;
  }
  if (inst.operands.size() != 1u) {
    return false;
  }

  uint32_t source_id = 0;
  if (!MaterializeValue(inst.operands[0], &source_id)) {
    return false;
  }

  SpvOp op = SpvOpNop;
  if (!ResolveCastOpcode(inst.operands[0].type, inst.result_type,
                         type_emitter_->GetTypeTable(), &op)) {
    return false;
  }

  uint32_t result_id = ids_.Allocate();
  AppendInstruction(&sections_->functions, op,
                    {GetSpirvTypeId(inst.result_type), result_id, source_id});
  value_map_[inst.result_id] = result_id;
  return true;
}

bool ModuleBuilder::EmitConstruct(const ir::Instruction& inst) {
  if (FindValue(inst.result_id) == values_.end()) return false;
  if (inst.operands.empty()) return false;

  std::vector<uint32_t> operands;
  operands.push_back(GetSpirvTypeId(inst.result_type));
  const uint32_t result_id = ids_.Allocate();
  operands.push_back(result_id);

  for (const auto& operand : inst.operands) {
    uint32_t component_id = 0;
    if (!MaterializeValue(operand, &component_id)) {
      return false;
    }
    operands.push_back(component_id);
  }

  AppendInstruction(&sections_->functions, SpvOpCompositeConstruct, operands);
  value_map_[inst.result_id] = result_id;
  return true;
}

bool ModuleBuilder::EmitCall(const ir::Instruction& inst) {
  const ir::Function* callee = FindFunctionByName(inst.callee_name);
  if (callee == nullptr) {
    return false;
  }

  std::vector<uint32_t> operands;
  const uint32_t result_type =
      type_emitter_->EmitType(GetSpirvFunctionReturnType(*callee));
  const uint32_t result_id =
      inst.result_id != 0 ? ids_.Allocate() : ids_.Allocate();
  const uint32_t callee_id = GetFunctionId(inst.callee_name);
  if (result_type == 0 || callee_id == 0) {
    return false;
  }

  operands.push_back(result_type);
  operands.push_back(result_id);
  operands.push_back(callee_id);
  for (const auto& operand : inst.operands) {
    uint32_t arg_id = 0;
    if (!MaterializeValue(operand, &arg_id)) {
      return false;
    }
    operands.push_back(arg_id);
  }

  AppendInstruction(&sections_->functions, SpvOpFunctionCall, operands);
  if (inst.result_id != 0) {
    value_map_[inst.result_id] = result_id;
  }
  return true;
}

bool ModuleBuilder::EmitBuiltinCall(const ir::Instruction& inst) {
  if (FindValue(inst.result_id) == values_.end()) {
    return false;
  }

  switch (inst.builtin_call) {
    case ir::BuiltinCallKind::kTextureDimensions: {
      if (inst.operands.size() != 2u) {
        return false;
      }

      uint32_t texture_id = 0;
      if (!MaterializeValue(inst.operands[0], &texture_id)) {
        return false;
      }

      uint32_t lod_id = 0;
      if (!MaterializeValue(inst.operands[1], &lod_id)) {
        return false;
      }

      uint32_t result_id = ids_.Allocate();
      AppendInstruction(
          &sections_->functions, SpvOpImageQuerySizeLod,
          {GetSpirvTypeId(inst.result_type), result_id, texture_id, lod_id});
      value_map_[inst.result_id] = result_id;
      return true;
    }
    case ir::BuiltinCallKind::kTextureSample: {
      if (inst.operands.size() != 3u) {
        return false;
      }

      const ir::Value& texture = inst.operands[0];
      const ir::Value& sampler = inst.operands[1];
      const ir::Value& coord = inst.operands[2];

      uint32_t texture_id = 0;
      uint32_t sampler_id = 0;
      uint32_t coord_id = 0;
      if (!MaterializeValue(texture, &texture_id) ||
          !MaterializeValue(sampler, &sampler_id) ||
          !MaterializeValue(coord, &coord_id)) {
        return false;
      }

      uint32_t sampled_image_type =
          type_emitter_->GetSampledImageType(texture.type);
      if (sampled_image_type == 0) {
        return false;
      }

      uint32_t sampled_image_id = ids_.Allocate();
      AppendInstruction(
          &sections_->functions, SpvOpSampledImage,
          {sampled_image_type, sampled_image_id, texture_id, sampler_id});

      uint32_t result_id = ids_.Allocate();
      AppendInstruction(&sections_->functions, SpvOpImageSampleImplicitLod,
                        {GetSpirvTypeId(inst.result_type), result_id,
                         sampled_image_id, coord_id});
      value_map_[inst.result_id] = result_id;
      return true;
    }
    case ir::BuiltinCallKind::kTextureSampleLevel: {
      if (inst.operands.size() != 4u) {
        return false;
      }

      const ir::Value& texture = inst.operands[0];
      const ir::Value& sampler = inst.operands[1];
      const ir::Value& coord = inst.operands[2];
      const ir::Value& level = inst.operands[3];

      uint32_t texture_id = 0;
      uint32_t sampler_id = 0;
      uint32_t coord_id = 0;
      uint32_t level_id = 0;
      if (!MaterializeValue(texture, &texture_id) ||
          !MaterializeValue(sampler, &sampler_id) ||
          !MaterializeValue(coord, &coord_id) ||
          !MaterializeValue(level, &level_id)) {
        return false;
      }

      uint32_t sampled_image_type =
          type_emitter_->GetSampledImageType(texture.type);
      if (sampled_image_type == 0) {
        return false;
      }

      uint32_t sampled_image_id = ids_.Allocate();
      AppendInstruction(
          &sections_->functions, SpvOpSampledImage,
          {sampled_image_type, sampled_image_id, texture_id, sampler_id});

      uint32_t result_id = ids_.Allocate();
      AppendInstruction(
          &sections_->functions, SpvOpImageSampleExplicitLod,
          {GetSpirvTypeId(inst.result_type), result_id, sampled_image_id,
           coord_id, static_cast<uint32_t>(SpvImageOperandsLodMask), level_id});
      value_map_[inst.result_id] = result_id;
      return true;
    }
    case ir::BuiltinCallKind::kTextureLoad: {
      if (inst.operands.size() != 3u) {
        return false;
      }

      const ir::Value& texture = inst.operands[0];
      const ir::Value& coord = inst.operands[1];
      const ir::Value& level = inst.operands[2];

      uint32_t texture_id = 0;
      uint32_t coord_id = 0;
      uint32_t level_id = 0;
      if (!MaterializeValue(texture, &texture_id) ||
          !MaterializeValue(coord, &coord_id) ||
          !MaterializeValue(level, &level_id)) {
        return false;
      }

      uint32_t result_id = ids_.Allocate();
      AppendInstruction(
          &sections_->functions, SpvOpImageFetch,
          {GetSpirvTypeId(inst.result_type), result_id, texture_id, coord_id,
           static_cast<uint32_t>(SpvImageOperandsLodMask), level_id});
      value_map_[inst.result_id] = result_id;
      return true;
    }
    case ir::BuiltinCallKind::kNone:
    default:
      return false;
  }
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

bool ModuleBuilder::MaterializeBinaryOperand(const ir::Value& value,
                                             ir::TypeId result_type,
                                             uint32_t* value_id) {
  if (value_id == nullptr) {
    return false;
  }
  if (value.type == result_type) {
    return MaterializeValue(value, value_id);
  }

  ir::TypeTable* type_table = type_emitter_->GetTypeTable();
  if (type_table == nullptr || !type_table->IsVectorType(result_type) ||
      !type_table->IsScalarType(value.type) ||
      type_table->GetComponentType(result_type) != value.type) {
    return false;
  }

  uint32_t scalar_id = 0;
  if (!MaterializeValue(value, &scalar_id)) {
    return false;
  }

  const uint32_t component_count =
      type_table->GetVectorComponentCount(result_type);
  if (component_count < 2u || component_count > 4u) {
    return false;
  }

  std::vector<uint32_t> operands = {GetSpirvTypeId(result_type),
                                    ids_.Allocate()};
  for (uint32_t i = 0; i < component_count; ++i) {
    operands.push_back(scalar_id);
  }

  AppendInstruction(&sections_->functions, SpvOpCompositeConstruct, operands);
  *value_id = operands[1];
  return true;
}

bool ModuleBuilder::MaterializeAddress(const ir::Value& value,
                                       ir::TypeId pointee_type,
                                       uint32_t* ptr_id) {
  if (ptr_id == nullptr || !value.IsAddress()) {
    return false;
  }
  return GetAddressPointer(value, pointee_type, ptr_id) != 0;
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
  if (function_id_ != GetFunctionId(module_.entry_point)) {
    if (inst.operands.empty()) {
      AppendInstruction(&sections_->functions, SpvOpReturn, {});
      return true;
    }

    if (inst.operands.size() != 1 || !inst.operands[0].IsValue()) return false;

    uint32_t return_value = 0;
    if (!MaterializeValue(inst.operands[0], &return_value)) {
      return false;
    }

    AppendInstruction(&sections_->functions, SpvOpReturnValue, {return_value});
    return true;
  }

  if (inst.operands.empty()) {
    AppendInstruction(&sections_->functions, SpvOpReturn, {});
    return true;
  }

  if (inst.operands.size() != 1 || !inst.operands[0].IsValue()) {
    return false;
  }

  uint32_t return_value_id = 0;
  if (!MaterializeValue(inst.operands[0], &return_value_id)) {
    return false;
  }

  const ir::Type* return_type =
      type_emitter_->GetTypeTable()->GetType(inst.operands[0].type);
  const bool is_struct_return =
      return_type != nullptr && return_type->kind == ir::TypeKind::kStruct;

  for (const auto& output : output_vars_) {
    uint32_t value_to_store = return_value_id;
    if (is_struct_return) {
      if (!output.member_index.has_value()) {
        return false;
      }
      uint32_t extracted_id = ids_.Allocate();
      AppendInstruction(&sections_->functions, SpvOpCompositeExtract,
                        {GetSpirvTypeId(output.ir_type), extracted_id,
                         return_value_id, output.member_index.value()});
      value_to_store = extracted_id;
    }

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
  if (decoration_kind == ir::InterfaceDecorationKind::kBuiltin) {
    return static_cast<ir::BuiltinType>(decoration_value);
  }
  return ir::BuiltinType::kNone;
}

uint32_t ModuleBuilder::OutputVarInfo::GetLocation() const {
  if (decoration_kind == ir::InterfaceDecorationKind::kLocation) {
    return decoration_value;
  }
  return 0;
}

ir::BuiltinType ModuleBuilder::InputVarInfo::GetBuiltin() const {
  if (decoration_kind == ir::InterfaceDecorationKind::kBuiltin) {
    return static_cast<ir::BuiltinType>(decoration_value);
  }
  return ir::BuiltinType::kNone;
}

uint32_t ModuleBuilder::InputVarInfo::GetLocation() const {
  if (decoration_kind == ir::InterfaceDecorationKind::kLocation) {
    return decoration_value;
  }
  return 0;
}

uint32_t ModuleBuilder::GetFunctionId(std::string_view function_name) const {
  auto it = function_id_map_.find(std::string(function_name));
  return it == function_id_map_.end() ? 0 : it->second;
}

uint32_t ModuleBuilder::GetFunctionTypeId(
    std::string_view function_name) const {
  auto it = function_type_id_map_.find(std::string(function_name));
  return it == function_type_id_map_.end() ? 0 : it->second;
}

ir::TypeId ModuleBuilder::GetSpirvFunctionReturnType(
    const ir::Function& function) const {
  if (function.name == module_.entry_point) {
    return type_emitter_->GetTypeTable()->GetVoidType();
  }
  if (function.return_type == ir::kInvalidTypeId) {
    return type_emitter_->GetTypeTable()->GetVoidType();
  }
  return function.return_type;
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

const ModuleBuilder::InputVarInfo* ModuleBuilder::FindInputVarInfoByTarget(
    uint32_t ir_var_id) const {
  for (const auto& input : input_vars_) {
    if (input.target_var_id == ir_var_id) {
      return &input;
    }
  }
  return nullptr;
}

}  // namespace spirv
}  // namespace wgx
