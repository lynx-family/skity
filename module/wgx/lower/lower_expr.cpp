// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <string_view>

#include "lower/lower_internal.h"

namespace wgx {
namespace lower {

namespace {

bool IsComparisonOp(ast::BinaryOp op) {
  switch (op) {
    case ast::BinaryOp::kEqual:
    case ast::BinaryOp::kNotEqual:
    case ast::BinaryOp::kLessThan:
    case ast::BinaryOp::kGreaterThan:
    case ast::BinaryOp::kLessThanEqual:
    case ast::BinaryOp::kGreaterThanEqual:
      return true;
    default:
      return false;
  }
}

bool SupportsComparisonForType(ast::BinaryOp op, ir::TypeId type_id,
                               ir::TypeTable* type_table) {
  if (type_table == nullptr || !type_table->IsScalarType(type_id)) {
    return false;
  }

  if (type_table->GetBoolType() == type_id) {
    return op == ast::BinaryOp::kEqual || op == ast::BinaryOp::kNotEqual;
  }

  return type_table->IsIntegerType(type_id) || type_table->IsFloatType(type_id);
}

}  // namespace

ir::Value Lowerer::EnsureValue(const ir::ExprResult& expr, ir::Block* block) {
  if (!expr.IsAddress()) {
    return expr.value;
  }

  uint32_t load_id = AllocateSSAId();
  ir::Instruction load_inst;
  load_inst.kind = ir::InstKind::kLoad;
  load_inst.result_id = load_id;
  load_inst.result_type = expr.GetType();
  load_inst.operands.push_back(expr.value);
  block->instructions.emplace_back(load_inst);
  return ir::Value::SSA(expr.GetType(), load_id);
}

bool Lowerer::EmitStore(const ir::ExprResult& source_expr,
                        const ir::Value& target, ir::Block* block) {
  if (block == nullptr || !target.IsAddress()) {
    return false;
  }

  ir::Value source_value = EnsureValue(source_expr, block);
  if (!source_value.IsValue()) {
    return false;
  }

  ir::Instruction store_inst;
  store_inst.kind = ir::InstKind::kStore;
  store_inst.operands.push_back(target);
  store_inst.operands.push_back(source_value);

  block->instructions.emplace_back(store_inst);
  return true;
}

ir::ExprResult Lowerer::LowerExpression(ast::Expression* expression) {
  if (expression == nullptr) {
    return ir::ExprResult();
  }

  ir::ExprResult const_result = LowerConstant(expression);
  if (const_result.IsValid()) {
    return const_result;
  }

  if (expression->GetType() == ast::ExpressionType::kIdentifier) {
    return LowerIdentifierExpression(
        static_cast<ast::IdentifierExp*>(expression));
  }

  if (expression->GetType() == ast::ExpressionType::kBinaryExp) {
    return LowerBinaryExpression(static_cast<ast::BinaryExp*>(expression));
  }

  if (expression->GetType() == ast::ExpressionType::kMemberAccessor) {
    return LowerMemberAccessorExpression(
        static_cast<ast::MemberAccessor*>(expression));
  }

  if (expression->GetType() == ast::ExpressionType::kFuncCall) {
    return LowerFunctionCallExpression(
        static_cast<ast::FunctionCallExp*>(expression), CurrentBlock());
  }

  if (expression->GetType() == ast::ExpressionType::kParenExp) {
    auto* paren_exp = static_cast<ast::ParenExp*>(expression);
    if (paren_exp->exps.empty()) {
      return ir::ExprResult();
    }
    return LowerExpression(paren_exp->exps[0]);
  }

  return ir::ExprResult();
}

ir::ExprResult Lowerer::LowerConstant(ast::Expression* expression) {
  if (expression == nullptr) {
    return ir::ExprResult();
  }

  if (expression->GetType() == ast::ExpressionType::kFloatLiteral) {
    auto* lit = static_cast<ast::FloatLiteralExp*>(expression);
    ir::TypeId f32_type = type_table_->GetF32Type();
    return ir::ExprResult::ValueResult(
        ir::Value::ConstantF32(f32_type, static_cast<float>(lit->value)));
  }

  if (expression->GetType() == ast::ExpressionType::kIntLiteral) {
    auto* lit = static_cast<ast::IntLiteralExp*>(expression);
    ir::TypeId i32_type = type_table_->GetI32Type();
    return ir::ExprResult::ValueResult(
        ir::Value::ConstantI32(i32_type, static_cast<int32_t>(lit->value)));
  }

  if (expression->GetType() == ast::ExpressionType::kBoolLiteral) {
    auto* lit = static_cast<ast::BoolLiteralExp*>(expression);
    ir::TypeId bool_type = type_table_->GetBoolType();
    return ir::ExprResult::ValueResult(
        ir::Value::ConstantBool(bool_type, lit->value));
  }

  if (expression->GetType() == ast::ExpressionType::kFuncCall) {
    return LowerVectorConstructor(expression);
  }

  return ir::ExprResult();
}

ir::ExprResult Lowerer::LowerVectorConstructor(ast::Expression* expression) {
  if (expression == nullptr ||
      expression->GetType() != ast::ExpressionType::kFuncCall) {
    return ir::ExprResult();
  }

  auto* call = static_cast<ast::FunctionCallExp*>(expression);
  if (call->ident == nullptr) {
    return ir::ExprResult();
  }

  uint32_t count = detail::GetVectorComponentCount(call->ident);
  if (count < 2 || count > 4) {
    return ir::ExprResult();
  }

  const ast::IdentifierExp* scalar_ident =
      detail::GetVectorScalarType(call->ident);
  if (scalar_ident == nullptr) {
    return ir::ExprResult();
  }

  ir::TypeId component_type =
      detail::ResolveScalarType(scalar_ident, type_table_);
  if (component_type == ir::kInvalidTypeId) {
    return ir::ExprResult();
  }

  ir::TypeId vector_type = type_table_->GetVectorType(component_type, count);

  if (call->args.size() != count) {
    return ir::ExprResult();
  }

  bool all_constant = true;
  std::array<float, 4> values = {0.f, 0.f, 0.f, 0.f};
  for (size_t i = 0; i < count; ++i) {
    auto* arg = call->args[i];
    if (arg == nullptr) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_const = LowerConstant(arg);
    if (!arg_const.IsValid() || !arg_const.value.IsConstant()) {
      all_constant = false;
      break;
    }

    if (arg_const.GetType() != component_type) {
      return ir::ExprResult();
    }

    if (component_type == type_table_->GetF32Type()) {
      values[i] = arg_const.value.GetF32Unchecked();
    } else if (component_type == type_table_->GetI32Type()) {
      int32_t iv = arg_const.value.GetI32Unchecked();
      std::memcpy(&values[i], &iv, sizeof(iv));
    } else if (component_type == type_table_->GetU32Type()) {
      uint32_t uv = arg_const.value.GetU32Unchecked();
      std::memcpy(&values[i], &uv, sizeof(uv));
    } else if (component_type == type_table_->GetBoolType()) {
      bool bv = arg_const.value.GetBoolUnchecked();
      std::memcpy(&values[i], &bv, sizeof(bv));
    }
  }

  if (all_constant) {
    ir::Value result_value;
    switch (count) {
      case 2:
        result_value =
            ir::Value::ConstantVec2F32(vector_type, values[0], values[1]);
        break;
      case 3:
        result_value = ir::Value::ConstantVec3F32(vector_type, values[0],
                                                  values[1], values[2]);
        break;
      case 4:
        result_value = ir::Value::ConstantVec4F32(
            vector_type, values[0], values[1], values[2], values[3]);
        break;
      default:
        return ir::ExprResult();
    }

    return ir::ExprResult::ValueResult(result_value);
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Instruction construct_inst;
  construct_inst.kind = ir::InstKind::kConstruct;
  construct_inst.result_type = vector_type;
  construct_inst.result_id = AllocateSSAId();
  if (construct_inst.result_id == 0) {
    return ir::ExprResult();
  }

  for (auto* arg : call->args) {
    ir::ExprResult arg_expr = LowerExpression(arg);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }
    ir::Value arg_value = EnsureValue(arg_expr, current_block);
    if (!arg_value.IsValue() || arg_value.type != component_type) {
      return ir::ExprResult();
    }
    construct_inst.operands.push_back(arg_value);
  }

  current_block->instructions.emplace_back(construct_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(vector_type, construct_inst.result_id));
}

ir::ExprResult Lowerer::LowerBinaryExpression(ast::BinaryExp* binary) {
  if (binary == nullptr) {
    return ir::ExprResult();
  }

  ir::BinaryOpKind op_kind = ir::BinaryOpKind::kAdd;
  switch (binary->op) {
    case ast::BinaryOp::kAdd:
      op_kind = ir::BinaryOpKind::kAdd;
      break;
    case ast::BinaryOp::kSubtract:
      op_kind = ir::BinaryOpKind::kSubtract;
      break;
    case ast::BinaryOp::kEqual:
      op_kind = ir::BinaryOpKind::kEqual;
      break;
    case ast::BinaryOp::kNotEqual:
      op_kind = ir::BinaryOpKind::kNotEqual;
      break;
    case ast::BinaryOp::kLessThan:
      op_kind = ir::BinaryOpKind::kLessThan;
      break;
    case ast::BinaryOp::kGreaterThan:
      op_kind = ir::BinaryOpKind::kGreaterThan;
      break;
    case ast::BinaryOp::kLessThanEqual:
      op_kind = ir::BinaryOpKind::kLessThanEqual;
      break;
    case ast::BinaryOp::kGreaterThanEqual:
      op_kind = ir::BinaryOpKind::kGreaterThanEqual;
      break;
    default:
      return ir::ExprResult();
  }

  ir::ExprResult lhs_expr = LowerExpression(binary->lhs);
  if (!lhs_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::ExprResult rhs_expr = LowerExpression(binary->rhs);
  if (!rhs_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }
  ir::Value lhs_value = EnsureValue(lhs_expr, current_block);
  ir::Value rhs_value = EnsureValue(rhs_expr, current_block);

  if (lhs_value.type != rhs_value.type) {
    return ir::ExprResult();
  }

  ir::TypeId result_type = lhs_value.type;
  if (IsComparisonOp(binary->op)) {
    if (!SupportsComparisonForType(binary->op, lhs_value.type, type_table_)) {
      return ir::ExprResult();
    }
    result_type = type_table_->GetBoolType();
  }

  uint32_t result_id = AllocateSSAId();
  if (result_id == 0) {
    return ir::ExprResult();
  }

  ir::Instruction binary_inst;
  binary_inst.kind = ir::InstKind::kBinary;
  binary_inst.binary_op = op_kind;
  binary_inst.result_type = result_type;
  binary_inst.result_id = result_id;
  binary_inst.operands.push_back(lhs_value);
  binary_inst.operands.push_back(rhs_value);

  current_block->instructions.emplace_back(binary_inst);

  return ir::ExprResult::ValueResult(ir::Value::SSA(result_type, result_id));
}

ir::ExprResult Lowerer::LowerMemberAccessorExpression(
    ast::MemberAccessor* member) {
  if (member == nullptr || member->obj == nullptr ||
      member->member == nullptr) {
    return ir::ExprResult();
  }

  ir::ExprResult base_expr = LowerExpression(member->obj);
  if (!base_expr.IsValid()) {
    return ir::ExprResult();
  }

  uint32_t member_index = 0;
  const ir::StructMember* struct_member = FindStructMember(
      base_expr.GetType(), member->member->name, &member_index);
  if (struct_member == nullptr) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Instruction member_inst;
  member_inst.result_id = AllocateSSAId();
  member_inst.result_type = struct_member->type;
  member_inst.access_index = member_index;
  member_inst.operands.push_back(base_expr.value);
  if (member_inst.result_id == 0) {
    return ir::ExprResult();
  }

  if (base_expr.IsAddress()) {
    member_inst.kind = ir::InstKind::kAccess;
    current_block->instructions.emplace_back(member_inst);
    return ir::ExprResult::AddressResult(
        ir::Value::PointerSSA(struct_member->type, member_inst.result_id));
  }

  member_inst.kind = ir::InstKind::kExtract;
  current_block->instructions.emplace_back(member_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(struct_member->type, member_inst.result_id));
}

ir::ExprResult Lowerer::LowerFunctionCallExpression(ast::FunctionCallExp* call,
                                                    ir::Block* block) {
  if (call == nullptr || block == nullptr) {
    return ir::ExprResult();
  }

  const semantic::Symbol* callee_symbol = FindResolvedCallee(call);
  if (callee_symbol == nullptr) {
    return ir::ExprResult();
  }

  if (callee_symbol->kind == semantic::SymbolKind::kBuiltinFunction) {
    return LowerBuiltinCallExpression(call, callee_symbol, block);
  }

  const ast::Function* callee = FindResolvedFunction(call);
  if (callee == nullptr || !EnsureFunctionLowered(callee)) {
    return ir::ExprResult();
  }

  ir::Instruction call_inst;
  call_inst.kind = ir::InstKind::kCall;
  call_inst.callee_name = std::string{callee->name->name};

  for (auto* arg : call->args) {
    ir::ExprResult arg_expr = LowerExpression(arg);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue()) {
      return ir::ExprResult();
    }
    call_inst.operands.push_back(arg_value);
  }

  const ir::TypeId return_type = GetFunctionReturnType(callee);
  if (return_type != ir::kInvalidTypeId) {
    const uint32_t result_id = AllocateSSAId();
    if (result_id == 0) {
      return ir::ExprResult();
    }
    call_inst.result_type = return_type;
    call_inst.result_id = result_id;
    block->instructions.emplace_back(call_inst);
    return ir::ExprResult::ValueResult(ir::Value::SSA(return_type, result_id));
  }

  block->instructions.emplace_back(call_inst);
  return ir::ExprResult::ValueResult(ir::Value::None());
}

ir::ExprResult Lowerer::LowerBuiltinCallExpression(
    ast::FunctionCallExp* call, const semantic::Symbol* symbol,
    ir::Block* block) {
  if (call == nullptr || symbol == nullptr || block == nullptr) {
    return ir::ExprResult();
  }

  if (symbol->original_name == "textureDimensions") {
    if (call->args.empty() || call->args.size() > 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult texture_expr = LowerExpression(call->args[0]);
    if (!texture_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value texture_value = EnsureValue(texture_expr, block);
    if (!texture_value.IsValue()) {
      return ir::ExprResult();
    }

    const ir::Type* texture_type = type_table_->GetType(texture_value.type);
    if (texture_type == nullptr ||
        texture_type->kind != ir::TypeKind::kTexture2D) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kTextureDimensions;
    builtin_inst.result_type =
        type_table_->GetVectorType(type_table_->GetU32Type(), 2u);
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_type == ir::kInvalidTypeId ||
        builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(texture_value);

    if (call->args.size() == 2u) {
      ir::ExprResult lod_expr = LowerExpression(call->args[1]);
      if (!lod_expr.IsValid()) {
        return ir::ExprResult();
      }
      ir::Value lod_value = EnsureValue(lod_expr, block);
      if (!lod_value.IsValue()) {
        return ir::ExprResult();
      }
      builtin_inst.operands.push_back(lod_value);
    } else {
      builtin_inst.operands.push_back(
          ir::Value::ConstantI32(type_table_->GetI32Type(), 0));
    }

    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "textureSample") {
    if (call->args.size() != 3u) {
      return ir::ExprResult();
    }

    ir::ExprResult texture_expr = LowerExpression(call->args[0]);
    ir::ExprResult sampler_expr = LowerExpression(call->args[1]);
    ir::ExprResult coord_expr = LowerExpression(call->args[2]);
    if (!texture_expr.IsValid() || !sampler_expr.IsValid() ||
        !coord_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value texture_value = EnsureValue(texture_expr, block);
    ir::Value sampler_value = EnsureValue(sampler_expr, block);
    ir::Value coord_value = EnsureValue(coord_expr, block);
    if (!texture_value.IsValue() || !sampler_value.IsValue() ||
        !coord_value.IsValue()) {
      return ir::ExprResult();
    }

    const ir::Type* texture_type = type_table_->GetType(texture_value.type);
    const ir::Type* sampler_type = type_table_->GetType(sampler_value.type);
    if (texture_type == nullptr || sampler_type == nullptr ||
        texture_type->kind != ir::TypeKind::kTexture2D ||
        sampler_type->kind != ir::TypeKind::kSampler) {
      return ir::ExprResult();
    }

    const ir::TypeId coord_type =
        type_table_->GetVectorType(type_table_->GetF32Type(), 2u);
    if (coord_value.type != coord_type) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kTextureSample;
    builtin_inst.result_type =
        type_table_->GetVectorType(texture_type->element_type, 4u);
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_type == ir::kInvalidTypeId ||
        builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }

    builtin_inst.operands.push_back(texture_value);
    builtin_inst.operands.push_back(sampler_value);
    builtin_inst.operands.push_back(coord_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "textureSampleLevel") {
    if (call->args.size() != 4u) {
      return ir::ExprResult();
    }

    ir::ExprResult texture_expr = LowerExpression(call->args[0]);
    ir::ExprResult sampler_expr = LowerExpression(call->args[1]);
    ir::ExprResult coord_expr = LowerExpression(call->args[2]);
    ir::ExprResult level_expr = LowerExpression(call->args[3]);
    if (!texture_expr.IsValid() || !sampler_expr.IsValid() ||
        !coord_expr.IsValid() || !level_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value texture_value = EnsureValue(texture_expr, block);
    ir::Value sampler_value = EnsureValue(sampler_expr, block);
    ir::Value coord_value = EnsureValue(coord_expr, block);
    ir::Value level_value = EnsureValue(level_expr, block);
    if (!texture_value.IsValue() || !sampler_value.IsValue() ||
        !coord_value.IsValue() || !level_value.IsValue()) {
      return ir::ExprResult();
    }

    const ir::Type* texture_type = type_table_->GetType(texture_value.type);
    const ir::Type* sampler_type = type_table_->GetType(sampler_value.type);
    if (texture_type == nullptr || sampler_type == nullptr ||
        texture_type->kind != ir::TypeKind::kTexture2D ||
        sampler_type->kind != ir::TypeKind::kSampler) {
      return ir::ExprResult();
    }

    const ir::TypeId coord_type =
        type_table_->GetVectorType(type_table_->GetF32Type(), 2u);
    if (coord_value.type != coord_type ||
        level_value.type != type_table_->GetF32Type()) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kTextureSampleLevel;
    builtin_inst.result_type =
        type_table_->GetVectorType(texture_type->element_type, 4u);
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_type == ir::kInvalidTypeId ||
        builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }

    builtin_inst.operands.push_back(texture_value);
    builtin_inst.operands.push_back(sampler_value);
    builtin_inst.operands.push_back(coord_value);
    builtin_inst.operands.push_back(level_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name != "textureLoad" || call->args.size() != 3u) {
    return ir::ExprResult();
  }

  ir::ExprResult texture_expr = LowerExpression(call->args[0]);
  ir::ExprResult coord_expr = LowerExpression(call->args[1]);
  ir::ExprResult level_expr = LowerExpression(call->args[2]);
  if (!texture_expr.IsValid() || !coord_expr.IsValid() ||
      !level_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::Value texture_value = EnsureValue(texture_expr, block);
  ir::Value coord_value = EnsureValue(coord_expr, block);
  ir::Value level_value = EnsureValue(level_expr, block);
  if (!texture_value.IsValue() || !coord_value.IsValue() ||
      !level_value.IsValue()) {
    return ir::ExprResult();
  }

  const ir::Type* texture_type = type_table_->GetType(texture_value.type);
  if (texture_type == nullptr ||
      texture_type->kind != ir::TypeKind::kTexture2D) {
    return ir::ExprResult();
  }

  const ir::TypeId coord_type =
      type_table_->GetVectorType(type_table_->GetI32Type(), 2u);
  if (coord_value.type != coord_type ||
      level_value.type != type_table_->GetI32Type()) {
    return ir::ExprResult();
  }

  ir::Instruction builtin_inst;
  builtin_inst.kind = ir::InstKind::kBuiltinCall;
  builtin_inst.builtin_call = ir::BuiltinCallKind::kTextureLoad;
  builtin_inst.result_type =
      type_table_->GetVectorType(texture_type->element_type, 4u);
  builtin_inst.result_id = AllocateSSAId();
  if (builtin_inst.result_type == ir::kInvalidTypeId ||
      builtin_inst.result_id == 0) {
    return ir::ExprResult();
  }

  builtin_inst.operands.push_back(texture_value);
  builtin_inst.operands.push_back(coord_value);
  builtin_inst.operands.push_back(level_value);
  block->instructions.emplace_back(builtin_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
}

ir::ExprResult Lowerer::LowerIdentifierExpression(ast::IdentifierExp* ident) {
  if (ident->ident == nullptr) {
    return ir::ExprResult();
  }
  const semantic::Symbol* symbol = FindResolvedSymbol(ident);
  if (symbol == nullptr) {
    return ir::ExprResult();
  }

  auto var_info = LookupVar(symbol);
  if (var_info.id == 0) {
    var_info = LookupOrRegisterGlobalVar(symbol);
    if (var_info.id == 0) {
      return ir::ExprResult();
    }
  }

  return ir::ExprResult::AddressResult(
      ir::Value::Variable(var_info.type, var_info.id));
}

uint32_t Lowerer::AllocateVarId() {
  return ir_function_ ? ir_function_->AllocateVarId() : 0;
}

uint32_t Lowerer::AllocateSSAId() {
  return ir_function_ ? ir_function_->AllocateSSAId() : 0;
}

bool Lowerer::RegisterVar(const semantic::Symbol* symbol, uint32_t id,
                          ir::TypeId type) {
  if (symbol == nullptr) {
    return false;
  }
  auto [it, inserted] = var_map_.emplace(symbol, VarInfo{id, type});
  return inserted;
}

Lowerer::VarInfo Lowerer::LookupVar(const semantic::Symbol* symbol) const {
  if (symbol == nullptr) {
    return VarInfo{};
  }
  auto it = var_map_.find(symbol);
  return it == var_map_.end() ? VarInfo{} : it->second;
}

Lowerer::VarInfo Lowerer::LookupOrRegisterGlobalVar(
    const semantic::Symbol* symbol) {
  if (symbol == nullptr || symbol->declaration == nullptr) {
    return VarInfo{};
  }

  const ast::Node* decl = symbol->declaration;
  ast::Variable* global_var = nullptr;

  for (auto* var : ast_module_->global_declarations) {
    if (var == decl) {
      global_var = var;
      break;
    }
  }

  if (global_var == nullptr) {
    return VarInfo{};
  }

  auto it = var_map_.find(symbol);
  if (it != var_map_.end()) {
    return it->second;
  }

  ir::TypeId var_type = ResolveType(global_var->type);
  if (var_type == ir::kInvalidTypeId) {
    return VarInfo{};
  }

  const uint32_t var_id = AllocateVarId();
  if (var_id == 0) {
    return VarInfo{};
  }

  if (!RegisterVar(symbol, var_id, var_type)) {
    return VarInfo{};
  }

  ir::Module::GlobalVariable global_var_info;
  global_var_info.type = var_type;
  global_var_info.storage_class = ir::StorageClass::kPrivate;

  const ir::Type* resolved_type = type_table_->GetType(var_type);
  if (resolved_type != nullptr &&
      (resolved_type->kind == ir::TypeKind::kSampler ||
       resolved_type->kind == ir::TypeKind::kTexture2D)) {
    global_var_info.storage_class = ir::StorageClass::kHandle;
  }

  if (global_var->GetType() == ast::VariableType::kVar) {
    auto* var_node = static_cast<ast::Var*>(global_var);
    if (var_node->address_space != nullptr &&
        var_node->address_space->GetType() ==
            ast::ExpressionType::kIdentifier) {
      auto* as_ident =
          static_cast<ast::IdentifierExp*>(var_node->address_space);
      if (as_ident->ident != nullptr) {
        std::string_view space = as_ident->ident->name;
        if (space == "uniform") {
          global_var_info.storage_class = ir::StorageClass::kUniform;
        } else if (space == "storage") {
          global_var_info.storage_class = ir::StorageClass::kStorage;
        } else if (space == "workgroup") {
          global_var_info.storage_class = ir::StorageClass::kWorkgroup;
        } else if (space == "private") {
          global_var_info.storage_class = ir::StorageClass::kPrivate;
        }
      }
    }
  }

  if (global_var_info.storage_class == ir::StorageClass::kUniform ||
      global_var_info.storage_class == ir::StorageClass::kStorage) {
    const ir::Type* type = type_table_->GetType(var_type);
    if (type != nullptr && type->kind != ir::TypeKind::kStruct) {
      global_var_info.inner_type = var_type;

      ir::TypeTable::LayoutRule layout_rule =
          (global_var_info.storage_class == ir::StorageClass::kUniform)
              ? ir::TypeTable::LayoutRule::kStd140
              : ir::TypeTable::LayoutRule::kStd430;

      auto member_layout = type_table_->GetLayoutInfo(var_type, layout_rule);
      uint32_t member_offset =
          ir::TypeTable::AlignOffset(0, member_layout.alignment);

      std::vector<ir::StructMember> members;
      members.push_back(ir::StructMember{var_type, "value", member_offset});
      ir::TypeId struct_type = type_table_->GetStructType(members);
      global_var_info.type = struct_type;
    }
  }

  for (auto* attr : global_var->attributes) {
    if (attr == nullptr) continue;
    if (attr->GetType() == ast::AttributeType::kGroup) {
      auto* group_attr = static_cast<ast::GroupAttribute*>(attr);
      global_var_info.group = static_cast<uint32_t>(group_attr->index);
    } else if (attr->GetType() == ast::AttributeType::kBinding) {
      auto* binding_attr = static_cast<ast::BindingAttribute*>(attr);
      global_var_info.binding = static_cast<uint32_t>(binding_attr->index);
    }
  }

  if (global_var->initializer != nullptr) {
    ir::ExprResult init_expr = LowerExpression(global_var->initializer);
    if (init_expr.IsValid() && init_expr.value.IsConstant()) {
      global_var_info.initializer = init_expr.value;
    }
  }

  ir_module_->global_variables[var_id] = std::move(global_var_info);
  return VarInfo{var_id, var_type};
}

}  // namespace lower
}  // namespace wgx
