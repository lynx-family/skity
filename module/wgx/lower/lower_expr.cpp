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

bool SupportsShiftForType(ir::TypeId type_id, ir::TypeTable* type_table) {
  if (type_table == nullptr) {
    return false;
  }

  if (type_table->IsIntegerType(type_id)) {
    return true;
  }

  if (!type_table->IsVectorType(type_id)) {
    return false;
  }

  const ir::TypeId component_type = type_table->GetComponentType(type_id);
  return type_table->IsIntegerType(component_type);
}

bool SupportsBitwiseForType(ir::TypeId type_id, ir::TypeTable* type_table) {
  if (type_table == nullptr) {
    return false;
  }

  if (type_table->IsIntegerType(type_id)) {
    return true;
  }

  if (!type_table->IsVectorType(type_id)) {
    return false;
  }

  return type_table->IsIntegerType(type_table->GetComponentType(type_id));
}

bool SupportsLogicalForType(ir::TypeId type_id, ir::TypeTable* type_table) {
  if (type_table == nullptr) {
    return false;
  }

  if (type_id == type_table->GetBoolType()) {
    return true;
  }

  if (!type_table->IsVectorType(type_id)) {
    return false;
  }

  return type_table->GetComponentType(type_id) == type_table->GetBoolType();
}

bool SupportsNegationForType(ir::TypeId type_id, ir::TypeTable* type_table) {
  if (type_table == nullptr) {
    return false;
  }

  if (type_table->IsFloatType(type_id) || type_table->IsIntegerType(type_id)) {
    return true;
  }

  if (!type_table->IsVectorType(type_id)) {
    return false;
  }

  const ir::TypeId component_type = type_table->GetComponentType(type_id);
  return type_table->IsFloatType(component_type) ||
         type_table->IsIntegerType(component_type);
}

bool IsVectorScalarBinary(ast::BinaryOp op, ir::TypeId lhs_type,
                          ir::TypeId rhs_type, ir::TypeTable* type_table,
                          ir::TypeId* result_type) {
  if (type_table == nullptr || result_type == nullptr) {
    return false;
  }

  if (op != ast::BinaryOp::kMultiply && op != ast::BinaryOp::kDivide) {
    return false;
  }

  const bool lhs_is_vector = type_table->IsVectorType(lhs_type);
  const bool rhs_is_vector = type_table->IsVectorType(rhs_type);
  const bool lhs_is_scalar = type_table->IsScalarType(lhs_type);
  const bool rhs_is_scalar = type_table->IsScalarType(rhs_type);

  if (lhs_is_vector && rhs_is_scalar) {
    if (type_table->GetComponentType(lhs_type) != rhs_type) {
      return false;
    }
    *result_type = lhs_type;
    return true;
  }

  if (op == ast::BinaryOp::kMultiply && lhs_is_scalar && rhs_is_vector) {
    if (type_table->GetComponentType(rhs_type) != lhs_type) {
      return false;
    }
    *result_type = rhs_type;
    return true;
  }

  return false;
}

bool IsMatrixVectorBinary(ast::BinaryOp op, ir::TypeId lhs_type,
                          ir::TypeId rhs_type, ir::TypeTable* type_table,
                          ir::TypeId* result_type) {
  if (type_table == nullptr || result_type == nullptr ||
      op != ast::BinaryOp::kMultiply) {
    return false;
  }

  if (!type_table->IsMatrixType(lhs_type) ||
      !type_table->IsVectorType(rhs_type)) {
    return false;
  }

  const ir::Type* matrix_type = type_table->GetType(lhs_type);
  if (matrix_type == nullptr ||
      matrix_type->element_type != type_table->GetF32Type()) {
    return false;
  }

  if (type_table->GetComponentType(rhs_type) != type_table->GetF32Type()) {
    return false;
  }

  const uint32_t columns = matrix_type->count2;
  const uint32_t rows = matrix_type->count;
  if (columns != type_table->GetVectorComponentCount(rhs_type)) {
    return false;
  }

  *result_type = type_table->GetVectorType(type_table->GetF32Type(), rows);
  return *result_type != ir::kInvalidTypeId;
}

bool IsMatrixMatrixBinary(ast::BinaryOp op, ir::TypeId lhs_type,
                          ir::TypeId rhs_type, ir::TypeTable* type_table,
                          ir::TypeId* result_type) {
  if (type_table == nullptr || result_type == nullptr ||
      op != ast::BinaryOp::kMultiply) {
    return false;
  }

  if (!type_table->IsMatrixType(lhs_type) ||
      !type_table->IsMatrixType(rhs_type)) {
    return false;
  }

  const ir::Type* lhs_matrix_type = type_table->GetType(lhs_type);
  const ir::Type* rhs_matrix_type = type_table->GetType(rhs_type);
  if (lhs_matrix_type == nullptr || rhs_matrix_type == nullptr ||
      lhs_matrix_type->element_type != type_table->GetF32Type() ||
      rhs_matrix_type->element_type != type_table->GetF32Type()) {
    return false;
  }

  if (lhs_matrix_type->count2 != rhs_matrix_type->count) {
    return false;
  }

  *result_type = type_table->GetMatrixType(type_table->GetF32Type(),
                                           lhs_matrix_type->count,
                                           rhs_matrix_type->count2);
  return *result_type != ir::kInvalidTypeId;
}

bool IsSupportedScalarCastType(ir::TypeId type_id, ir::TypeTable* type_table) {
  if (type_table == nullptr || type_id == ir::kInvalidTypeId) {
    return false;
  }
  return type_id == type_table->GetF32Type() ||
         type_id == type_table->GetI32Type() ||
         type_id == type_table->GetU32Type();
}

std::optional<ir::Value> FoldScalarCastConstant(const ir::Value& value,
                                                ir::TypeId result_type,
                                                ir::TypeTable* type_table) {
  if (type_table == nullptr || !value.IsConstant() ||
      !IsSupportedScalarCastType(result_type, type_table) ||
      !IsSupportedScalarCastType(value.type, type_table)) {
    return std::nullopt;
  }

  if (value.type == result_type) {
    return value;
  }

  if (result_type == type_table->GetF32Type()) {
    if (value.type == type_table->GetI32Type()) {
      return ir::Value::ConstantF32(
          result_type, static_cast<float>(value.GetI32Unchecked()));
    }
    if (value.type == type_table->GetU32Type()) {
      return ir::Value::ConstantF32(
          result_type, static_cast<float>(value.GetU32Unchecked()));
    }
    return std::nullopt;
  }

  if (result_type == type_table->GetI32Type()) {
    if (value.type == type_table->GetF32Type()) {
      return ir::Value::ConstantI32(
          result_type, static_cast<int32_t>(value.GetF32Unchecked()));
    }
    if (value.type == type_table->GetU32Type()) {
      return ir::Value::ConstantI32(
          result_type, static_cast<int32_t>(value.GetU32Unchecked()));
    }
    return std::nullopt;
  }

  if (result_type == type_table->GetU32Type()) {
    if (value.type == type_table->GetF32Type()) {
      return ir::Value::ConstantU32(
          result_type, static_cast<uint32_t>(value.GetF32Unchecked()));
    }
    if (value.type == type_table->GetI32Type()) {
      return ir::Value::ConstantU32(
          result_type, static_cast<uint32_t>(value.GetI32Unchecked()));
    }
    return std::nullopt;
  }

  return std::nullopt;
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

  if (expression->GetType() == ast::ExpressionType::kUnaryExp) {
    return LowerUnaryExpression(static_cast<ast::UnaryExp*>(expression));
  }

  if (expression->GetType() == ast::ExpressionType::kIndexAccessor) {
    return LowerIndexAccessorExpression(
        static_cast<ast::IndexAccessorExp*>(expression));
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

ir::ExprResult Lowerer::LowerUnaryExpression(ast::UnaryExp* unary) {
  if (unary == nullptr || unary->exp == nullptr) {
    return ir::ExprResult();
  }

  ir::ExprResult operand_expr = LowerExpression(unary->exp);
  if (!operand_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Value operand_value = EnsureValue(operand_expr, current_block);
  if (!operand_value.IsValue()) {
    return ir::ExprResult();
  }

  if (unary->op == ast::UnaryOp::kNot) {
    if (operand_value.type != type_table_->GetBoolType()) {
      return ir::ExprResult();
    }

    uint32_t result_id = AllocateSSAId();
    if (result_id == 0) {
      return ir::ExprResult();
    }

    ir::Instruction not_inst;
    not_inst.kind = ir::InstKind::kBinary;
    not_inst.binary_op = ir::BinaryOpKind::kEqual;
    not_inst.result_type = type_table_->GetBoolType();
    not_inst.result_id = result_id;
    not_inst.operands.push_back(operand_value);
    not_inst.operands.push_back(
        ir::Value::ConstantBool(type_table_->GetBoolType(), false));
    current_block->instructions.emplace_back(not_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(type_table_->GetBoolType(), result_id));
  }

  if (unary->op != ast::UnaryOp::kNegation ||
      !SupportsNegationForType(operand_value.type, type_table_)) {
    return ir::ExprResult();
  }

  ir::Value zero_value;
  if (type_table_->IsVectorType(operand_value.type)) {
    const ir::TypeId component_type =
        type_table_->GetComponentType(operand_value.type);
    const uint32_t component_count =
        type_table_->GetVectorComponentCount(operand_value.type);

    ir::Instruction zero_construct;
    zero_construct.kind = ir::InstKind::kConstruct;
    zero_construct.result_type = operand_value.type;
    zero_construct.result_id = AllocateSSAId();
    if (zero_construct.result_id == 0) {
      return ir::ExprResult();
    }

    for (uint32_t i = 0; i < component_count; ++i) {
      if (component_type == type_table_->GetF32Type()) {
        zero_construct.operands.push_back(
            ir::Value::ConstantF32(component_type, 0.0f));
      } else if (component_type == type_table_->GetI32Type()) {
        zero_construct.operands.push_back(
            ir::Value::ConstantI32(component_type, 0));
      } else if (component_type == type_table_->GetU32Type()) {
        zero_construct.operands.push_back(
            ir::Value::ConstantU32(component_type, 0u));
      } else {
        return ir::ExprResult();
      }
    }

    current_block->instructions.emplace_back(zero_construct);
    zero_value = ir::Value::SSA(operand_value.type, zero_construct.result_id);
  } else if (operand_value.type == type_table_->GetF32Type()) {
    zero_value = ir::Value::ConstantF32(operand_value.type, 0.0f);
  } else if (operand_value.type == type_table_->GetI32Type()) {
    zero_value = ir::Value::ConstantI32(operand_value.type, 0);
  } else if (operand_value.type == type_table_->GetU32Type()) {
    zero_value = ir::Value::ConstantU32(operand_value.type, 0u);
  } else {
    return ir::ExprResult();
  }

  uint32_t result_id = AllocateSSAId();
  if (result_id == 0) {
    return ir::ExprResult();
  }

  ir::Instruction negate_inst;
  negate_inst.kind = ir::InstKind::kBinary;
  negate_inst.binary_op = ir::BinaryOpKind::kSubtract;
  negate_inst.result_type = operand_value.type;
  negate_inst.result_id = result_id;
  negate_inst.operands.push_back(zero_value);
  negate_inst.operands.push_back(operand_value);
  current_block->instructions.emplace_back(negate_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(operand_value.type, result_id));
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
    ir::ExprResult vector_result = LowerVectorConstructor(expression);
    if (vector_result.IsValid()) {
      return vector_result;
    }
    ir::ExprResult matrix_result = LowerMatrixConstructor(expression);
    if (matrix_result.IsValid()) {
      return matrix_result;
    }
    ir::ExprResult array_result = LowerArrayConstructor(expression);
    if (array_result.IsValid()) {
      return array_result;
    }
    return LowerScalarCastConstructor(expression);
  }

  return ir::ExprResult();
}

ir::ExprResult Lowerer::LowerArrayConstructor(ast::Expression* expression) {
  if (expression == nullptr ||
      expression->GetType() != ast::ExpressionType::kFuncCall) {
    return ir::ExprResult();
  }

  auto* call = static_cast<ast::FunctionCallExp*>(expression);
  if (call->ident == nullptr || call->ident->ident == nullptr ||
      call->ident->ident->name != "array") {
    return ir::ExprResult();
  }

  ast::Type array_type_decl;
  array_type_decl.expr = call->ident;
  ir::TypeId array_type = ResolveType(array_type_decl);
  if (!type_table_->IsArrayType(array_type)) {
    return ir::ExprResult();
  }

  const ir::Type* array_desc = type_table_->GetType(array_type);
  if (array_desc == nullptr || call->args.size() != array_desc->count) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Instruction construct_inst;
  construct_inst.kind = ir::InstKind::kConstruct;
  construct_inst.result_type = array_type;
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
    if (!arg_value.IsValue() || arg_value.type != array_desc->element_type) {
      return ir::ExprResult();
    }
    construct_inst.operands.push_back(arg_value);
  }

  current_block->instructions.emplace_back(construct_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(array_type, construct_inst.result_id));
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

ir::ExprResult Lowerer::LowerScalarCastConstructor(
    ast::Expression* expression) {
  if (expression == nullptr ||
      expression->GetType() != ast::ExpressionType::kFuncCall) {
    return ir::ExprResult();
  }

  auto* call = static_cast<ast::FunctionCallExp*>(expression);
  if (call->ident == nullptr || call->ident->ident == nullptr ||
      call->args.size() != 1u) {
    return ir::ExprResult();
  }

  ir::TypeId result_type = detail::ResolveScalarType(call->ident, type_table_);
  if (!IsSupportedScalarCastType(result_type, type_table_)) {
    return ir::ExprResult();
  }

  ir::ExprResult arg_expr = LowerExpression(call->args[0]);
  if (!arg_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (arg_expr.IsAddress() && current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Value arg_value = EnsureValue(arg_expr, current_block);
  if (!arg_value.IsValue() ||
      !IsSupportedScalarCastType(arg_value.type, type_table_)) {
    return ir::ExprResult();
  }

  if (arg_value.type == result_type) {
    return ir::ExprResult::ValueResult(arg_value);
  }

  if (auto folded = FoldScalarCastConstant(arg_value, result_type, type_table_);
      folded.has_value()) {
    return ir::ExprResult::ValueResult(*folded);
  }

  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Instruction cast_inst;
  cast_inst.kind = ir::InstKind::kCast;
  cast_inst.result_type = result_type;
  cast_inst.result_id = AllocateSSAId();
  if (cast_inst.result_id == 0) {
    return ir::ExprResult();
  }
  cast_inst.operands.push_back(arg_value);
  current_block->instructions.emplace_back(cast_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(result_type, cast_inst.result_id));
}

ir::ExprResult Lowerer::LowerMatrixConstructor(ast::Expression* expression) {
  if (expression == nullptr ||
      expression->GetType() != ast::ExpressionType::kFuncCall) {
    return ir::ExprResult();
  }

  auto* call = static_cast<ast::FunctionCallExp*>(expression);
  if (call->ident == nullptr) {
    return ir::ExprResult();
  }

  const uint32_t rows = detail::GetMatrixRowCount(call->ident);
  const uint32_t cols = detail::GetMatrixColumnCount(call->ident);
  if (rows < 2u || rows > 4u || cols < 2u || cols > 4u) {
    return ir::ExprResult();
  }

  const ast::IdentifierExp* scalar_ident =
      detail::GetMatrixScalarType(call->ident);
  if (scalar_ident == nullptr) {
    return ir::ExprResult();
  }

  ir::TypeId component_type =
      detail::ResolveScalarType(scalar_ident, type_table_);
  if (component_type != type_table_->GetF32Type()) {
    return ir::ExprResult();
  }

  ir::TypeId column_type = type_table_->GetVectorType(component_type, rows);
  ir::TypeId matrix_type =
      type_table_->GetMatrixType(component_type, rows, cols);
  if (column_type == ir::kInvalidTypeId || matrix_type == ir::kInvalidTypeId) {
    return ir::ExprResult();
  }

  const bool scalar_form = call->args.size() == rows * cols;
  const bool column_form = call->args.size() == cols;
  if (!scalar_form && !column_form) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  std::vector<ir::Value> column_values;
  column_values.reserve(cols);

  if (column_form) {
    for (auto* arg : call->args) {
      ir::ExprResult arg_expr = LowerExpression(arg);
      if (!arg_expr.IsValid()) {
        return ir::ExprResult();
      }

      ir::Value arg_value = EnsureValue(arg_expr, current_block);
      if (!arg_value.IsValue() || arg_value.type != column_type) {
        return ir::ExprResult();
      }
      column_values.push_back(arg_value);
    }
  } else {
    for (uint32_t col = 0; col < cols; ++col) {
      ir::Instruction column_construct;
      column_construct.kind = ir::InstKind::kConstruct;
      column_construct.result_type = column_type;
      column_construct.result_id = AllocateSSAId();
      if (column_construct.result_id == 0) {
        return ir::ExprResult();
      }

      for (uint32_t row = 0; row < rows; ++row) {
        ast::Expression* arg = call->args[col * rows + row];
        ir::ExprResult arg_expr = LowerExpression(arg);
        if (!arg_expr.IsValid()) {
          return ir::ExprResult();
        }

        ir::Value arg_value = EnsureValue(arg_expr, current_block);
        if (!arg_value.IsValue() || arg_value.type != component_type) {
          return ir::ExprResult();
        }

        column_construct.operands.push_back(arg_value);
      }

      current_block->instructions.emplace_back(column_construct);
      column_values.push_back(
          ir::Value::SSA(column_type, column_construct.result_id));
    }
  }

  ir::Instruction matrix_construct;
  matrix_construct.kind = ir::InstKind::kConstruct;
  matrix_construct.result_type = matrix_type;
  matrix_construct.result_id = AllocateSSAId();
  if (matrix_construct.result_id == 0) {
    return ir::ExprResult();
  }
  matrix_construct.operands = std::move(column_values);
  current_block->instructions.emplace_back(matrix_construct);

  return ir::ExprResult::ValueResult(
      ir::Value::SSA(matrix_type, matrix_construct.result_id));
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
    case ast::BinaryOp::kMultiply:
      op_kind = ir::BinaryOpKind::kMultiply;
      break;
    case ast::BinaryOp::kDivide:
      op_kind = ir::BinaryOpKind::kDivide;
      break;
    case ast::BinaryOp::kAnd:
      op_kind = ir::BinaryOpKind::kBitwiseAnd;
      break;
    case ast::BinaryOp::kOr:
      op_kind = ir::BinaryOpKind::kBitwiseOr;
      break;
    case ast::BinaryOp::kXor:
      op_kind = ir::BinaryOpKind::kBitwiseXor;
      break;
    case ast::BinaryOp::kLogicalAnd:
      op_kind = ir::BinaryOpKind::kLogicalAnd;
      break;
    case ast::BinaryOp::kLogicalOr:
      op_kind = ir::BinaryOpKind::kLogicalOr;
      break;
    case ast::BinaryOp::kShiftLeft:
      op_kind = ir::BinaryOpKind::kShiftLeft;
      break;
    case ast::BinaryOp::kShiftRight:
      op_kind = ir::BinaryOpKind::kShiftRight;
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

  ir::TypeId result_type = lhs_value.type;
  if (lhs_value.type != rhs_value.type) {
    if (!IsVectorScalarBinary(binary->op, lhs_value.type, rhs_value.type,
                              type_table_, &result_type) &&
        !IsMatrixVectorBinary(binary->op, lhs_value.type, rhs_value.type,
                              type_table_, &result_type) &&
        !IsMatrixMatrixBinary(binary->op, lhs_value.type, rhs_value.type,
                              type_table_, &result_type)) {
      return ir::ExprResult();
    }
  } else {
    if (IsMatrixMatrixBinary(binary->op, lhs_value.type, rhs_value.type,
                             type_table_, &result_type)) {
      // result_type updated by helper
    } else {
      result_type = lhs_value.type;
    }
  }

  if (IsComparisonOp(binary->op)) {
    if (!SupportsComparisonForType(binary->op, lhs_value.type, type_table_)) {
      return ir::ExprResult();
    }
    result_type = type_table_->GetBoolType();
  } else if (binary->op == ast::BinaryOp::kLogicalAnd ||
             binary->op == ast::BinaryOp::kLogicalOr) {
    if (lhs_value.type != rhs_value.type ||
        !SupportsLogicalForType(lhs_value.type, type_table_)) {
      return ir::ExprResult();
    }
    result_type = lhs_value.type;
  } else if (binary->op == ast::BinaryOp::kAnd ||
             binary->op == ast::BinaryOp::kOr ||
             binary->op == ast::BinaryOp::kXor) {
    if (lhs_value.type != rhs_value.type ||
        !SupportsBitwiseForType(lhs_value.type, type_table_)) {
      return ir::ExprResult();
    }
    result_type = lhs_value.type;
  } else if (binary->op == ast::BinaryOp::kShiftLeft ||
             binary->op == ast::BinaryOp::kShiftRight) {
    if (lhs_value.type != rhs_value.type ||
        !SupportsShiftForType(lhs_value.type, type_table_)) {
      return ir::ExprResult();
    }
    result_type = lhs_value.type;
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

ir::ExprResult Lowerer::LowerIndexAccessorExpression(
    ast::IndexAccessorExp* index) {
  if (index == nullptr || index->obj == nullptr || index->idx == nullptr) {
    return ir::ExprResult();
  }

  ir::ExprResult base_expr = LowerExpression(index->obj);
  if (!base_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::ExprResult index_expr = LowerExpression(index->idx);
  if (!index_expr.IsValid()) {
    return ir::ExprResult();
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return ir::ExprResult();
  }

  ir::Value index_value = EnsureValue(index_expr, current_block);
  if (!index_value.IsValue() || !type_table_->IsIntegerType(index_value.type) ||
      !type_table_->IsScalarType(index_value.type)) {
    return ir::ExprResult();
  }

  const ir::TypeId result_type =
      type_table_->GetIndexedElementType(base_expr.GetType());
  if (result_type == ir::kInvalidTypeId) {
    return ir::ExprResult();
  }

  ir::Instruction access_inst;
  access_inst.result_id = AllocateSSAId();
  access_inst.result_type = result_type;
  if (access_inst.result_id == 0) {
    return ir::ExprResult();
  }

  bool has_constant_index = false;
  uint32_t constant_index = 0;
  if (index_value.IsConstant()) {
    if (index_value.type == type_table_->GetI32Type()) {
      const int32_t raw_index = index_value.GetI32Unchecked();
      if (raw_index >= 0) {
        has_constant_index = true;
        constant_index = static_cast<uint32_t>(raw_index);
      }
    } else if (index_value.type == type_table_->GetU32Type()) {
      has_constant_index = true;
      constant_index = index_value.GetU32Unchecked();
    }
  }

  if (base_expr.IsAddress() && has_constant_index &&
      (type_table_->IsVectorType(base_expr.GetType()) ||
       type_table_->IsMatrixType(base_expr.GetType()))) {
    ir::Value base_value = EnsureValue(base_expr, current_block);
    if (!base_value.IsValue()) {
      return ir::ExprResult();
    }
    access_inst.kind = ir::InstKind::kExtract;
    access_inst.operands.push_back(base_value);
    access_inst.access_index = constant_index;
    current_block->instructions.emplace_back(access_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(result_type, access_inst.result_id));
  }

  if (base_expr.IsAddress()) {
    access_inst.kind = ir::InstKind::kAccess;
    access_inst.operands.push_back(base_expr.value);
    if (has_constant_index) {
      access_inst.access_index = constant_index;
    } else {
      access_inst.operands.push_back(index_value);
    }
    current_block->instructions.emplace_back(access_inst);
    return ir::ExprResult::AddressResult(
        ir::Value::PointerSSA(result_type, access_inst.result_id));
  }

  access_inst.kind = ir::InstKind::kExtract;
  access_inst.operands.push_back(base_expr.value);
  if (has_constant_index) {
    access_inst.access_index = constant_index;
  } else {
    access_inst.operands.push_back(index_value);
  }
  current_block->instructions.emplace_back(access_inst);
  return ir::ExprResult::ValueResult(
      ir::Value::SSA(result_type, access_inst.result_id));
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

  const auto is_f32_or_f32_vector = [&](ir::TypeId type) {
    return type == type_table_->GetF32Type() ||
           (type_table_->IsVectorType(type) &&
            type_table_->GetComponentType(type) == type_table_->GetF32Type());
  };

  if (symbol->original_name == "atan") {
    if (call->args.size() != 1u && call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult lhs_expr = LowerExpression(call->args[0]);
    if (!lhs_expr.IsValid()) {
      return ir::ExprResult();
    }
    ir::Value lhs_value = EnsureValue(lhs_expr, block);
    if (!lhs_value.IsValue() || !is_f32_or_f32_vector(lhs_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = call->args.size() == 1u
                                    ? ir::BuiltinCallKind::kAtan
                                    : ir::BuiltinCallKind::kAtan2;
    builtin_inst.result_type = lhs_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(lhs_value);

    if (call->args.size() == 2u) {
      ir::ExprResult rhs_expr = LowerExpression(call->args[1]);
      if (!rhs_expr.IsValid()) {
        return ir::ExprResult();
      }
      ir::Value rhs_value = EnsureValue(rhs_expr, block);
      if (!rhs_value.IsValue() || rhs_value.type != lhs_value.type) {
        return ir::ExprResult();
      }
      builtin_inst.operands.push_back(rhs_value);
    }

    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "pow") {
    if (call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult x_expr = LowerExpression(call->args[0]);
    ir::ExprResult y_expr = LowerExpression(call->args[1]);
    if (!x_expr.IsValid() || !y_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value x_value = EnsureValue(x_expr, block);
    ir::Value y_value = EnsureValue(y_expr, block);
    if (!x_value.IsValue() || !y_value.IsValue() ||
        x_value.type != y_value.type || !is_f32_or_f32_vector(x_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kPow;
    builtin_inst.result_type = x_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(x_value);
    builtin_inst.operands.push_back(y_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "exp" || symbol->original_name == "sin" ||
      symbol->original_name == "cos") {
    if (call->args.size() != 1u) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_expr = LowerExpression(call->args[0]);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue() || !is_f32_or_f32_vector(arg_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    if (symbol->original_name == "exp") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kExp;
    } else if (symbol->original_name == "sin") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kSin;
    } else {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kCos;
    }
    builtin_inst.result_type = arg_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(arg_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "dFdx" || symbol->original_name == "dpdx" ||
      symbol->original_name == "dFdy" || symbol->original_name == "dpdy") {
    if (call->args.size() != 1u) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_expr = LowerExpression(call->args[0]);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue() || !is_f32_or_f32_vector(arg_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call =
        (symbol->original_name == "dFdx" || symbol->original_name == "dpdx")
            ? ir::BuiltinCallKind::kDPdx
            : ir::BuiltinCallKind::kDPdy;
    builtin_inst.result_type = arg_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(arg_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "length") {
    if (call->args.size() != 1u) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_expr = LowerExpression(call->args[0]);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue() || !is_f32_or_f32_vector(arg_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kLength;
    builtin_inst.result_type = type_table_->GetF32Type();
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(arg_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "normalize") {
    if (call->args.size() != 1u) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_expr = LowerExpression(call->args[0]);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue() || !is_f32_or_f32_vector(arg_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kNormalize;
    builtin_inst.result_type = arg_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(arg_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "inverseSqrt" ||
      symbol->original_name == "inversesqrt") {
    if (call->args.size() != 1u) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_expr = LowerExpression(call->args[0]);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue() || !is_f32_or_f32_vector(arg_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kInverseSqrt;
    builtin_inst.result_type = arg_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(arg_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "mix") {
    if (call->args.size() != 3u) {
      return ir::ExprResult();
    }

    ir::ExprResult x_expr = LowerExpression(call->args[0]);
    ir::ExprResult y_expr = LowerExpression(call->args[1]);
    ir::ExprResult a_expr = LowerExpression(call->args[2]);
    if (!x_expr.IsValid() || !y_expr.IsValid() || !a_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value x_value = EnsureValue(x_expr, block);
    ir::Value y_value = EnsureValue(y_expr, block);
    ir::Value a_value = EnsureValue(a_expr, block);
    if (!x_value.IsValue() || !y_value.IsValue() || !a_value.IsValue() ||
        x_value.type != y_value.type || !is_f32_or_f32_vector(x_value.type)) {
      return ir::ExprResult();
    }

    const bool factor_same_type = a_value.type == x_value.type;
    const bool factor_scalar_for_vector =
        type_table_->IsVectorType(x_value.type) &&
        a_value.type == type_table_->GetF32Type();
    if (!factor_same_type && !factor_scalar_for_vector) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kMix;
    builtin_inst.result_type = x_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(x_value);
    builtin_inst.operands.push_back(y_value);
    builtin_inst.operands.push_back(a_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "step") {
    if (call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult edge_expr = LowerExpression(call->args[0]);
    ir::ExprResult x_expr = LowerExpression(call->args[1]);
    if (!edge_expr.IsValid() || !x_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value edge_value = EnsureValue(edge_expr, block);
    ir::Value x_value = EnsureValue(x_expr, block);
    if (!edge_value.IsValue() || !x_value.IsValue() ||
        !is_f32_or_f32_vector(x_value.type)) {
      return ir::ExprResult();
    }

    const bool edge_same_type = edge_value.type == x_value.type;
    const bool edge_scalar_for_vector =
        type_table_->IsVectorType(x_value.type) &&
        edge_value.type == type_table_->GetF32Type();
    if (!edge_same_type && !edge_scalar_for_vector) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kStep;
    builtin_inst.result_type = x_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(edge_value);
    builtin_inst.operands.push_back(x_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "smoothstep") {
    if (call->args.size() != 3u) {
      return ir::ExprResult();
    }

    ir::ExprResult edge0_expr = LowerExpression(call->args[0]);
    ir::ExprResult edge1_expr = LowerExpression(call->args[1]);
    ir::ExprResult x_expr = LowerExpression(call->args[2]);
    if (!edge0_expr.IsValid() || !edge1_expr.IsValid() || !x_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value edge0_value = EnsureValue(edge0_expr, block);
    ir::Value edge1_value = EnsureValue(edge1_expr, block);
    ir::Value x_value = EnsureValue(x_expr, block);
    if (!edge0_value.IsValue() || !edge1_value.IsValue() ||
        !x_value.IsValue() || !is_f32_or_f32_vector(x_value.type)) {
      return ir::ExprResult();
    }

    const bool edge0_ok = edge0_value.type == x_value.type ||
                          (type_table_->IsVectorType(x_value.type) &&
                           edge0_value.type == type_table_->GetF32Type());
    const bool edge1_ok = edge1_value.type == x_value.type ||
                          (type_table_->IsVectorType(x_value.type) &&
                           edge1_value.type == type_table_->GetF32Type());
    if (!edge0_ok || !edge1_ok) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kSmoothStep;
    builtin_inst.result_type = x_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(edge0_value);
    builtin_inst.operands.push_back(edge1_value);
    builtin_inst.operands.push_back(x_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "select") {
    if (call->args.size() != 3u) {
      return ir::ExprResult();
    }

    ir::ExprResult false_expr = LowerExpression(call->args[0]);
    ir::ExprResult true_expr = LowerExpression(call->args[1]);
    ir::ExprResult cond_expr = LowerExpression(call->args[2]);
    if (!false_expr.IsValid() || !true_expr.IsValid() || !cond_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value false_value = EnsureValue(false_expr, block);
    ir::Value true_value = EnsureValue(true_expr, block);
    ir::Value cond_value = EnsureValue(cond_expr, block);
    if (!false_value.IsValue() || !true_value.IsValue() ||
        !cond_value.IsValue() || false_value.type != true_value.type) {
      return ir::ExprResult();
    }

    bool cond_ok = false;
    if (false_value.type == type_table_->GetF32Type() ||
        false_value.type == type_table_->GetI32Type() ||
        false_value.type == type_table_->GetU32Type() ||
        false_value.type == type_table_->GetBoolType()) {
      cond_ok = cond_value.type == type_table_->GetBoolType();
    } else if (type_table_->IsVectorType(false_value.type)) {
      const uint32_t width =
          type_table_->GetVectorComponentCount(false_value.type);
      const ir::TypeId bool_vector_type =
          type_table_->GetVectorType(type_table_->GetBoolType(), width);
      cond_ok = cond_value.type == bool_vector_type;
    }

    if (!cond_ok) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kSelect;
    builtin_inst.result_type = false_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(false_value);
    builtin_inst.operands.push_back(true_value);
    builtin_inst.operands.push_back(cond_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "abs" || symbol->original_name == "sign" ||
      symbol->original_name == "sqrt" || symbol->original_name == "floor" ||
      symbol->original_name == "ceil" || symbol->original_name == "round") {
    if (call->args.size() != 1u) {
      return ir::ExprResult();
    }

    ir::ExprResult arg_expr = LowerExpression(call->args[0]);
    if (!arg_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value arg_value = EnsureValue(arg_expr, block);
    if (!arg_value.IsValue() || !is_f32_or_f32_vector(arg_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    if (symbol->original_name == "abs") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kAbs;
    } else if (symbol->original_name == "sign") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kSign;
    } else if (symbol->original_name == "floor") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kFloor;
    } else if (symbol->original_name == "ceil") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kCeil;
    } else if (symbol->original_name == "round") {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kRound;
    } else {
      builtin_inst.builtin_call = ir::BuiltinCallKind::kSqrt;
    }
    builtin_inst.result_type = arg_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(arg_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "max") {
    if (call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult lhs_expr = LowerExpression(call->args[0]);
    ir::ExprResult rhs_expr = LowerExpression(call->args[1]);
    if (!lhs_expr.IsValid() || !rhs_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value lhs_value = EnsureValue(lhs_expr, block);
    ir::Value rhs_value = EnsureValue(rhs_expr, block);
    if (!lhs_value.IsValue() || !rhs_value.IsValue() ||
        lhs_value.type != rhs_value.type ||
        !is_f32_or_f32_vector(lhs_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kMax;
    builtin_inst.result_type = lhs_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(lhs_value);
    builtin_inst.operands.push_back(rhs_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "min") {
    if (call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult lhs_expr = LowerExpression(call->args[0]);
    ir::ExprResult rhs_expr = LowerExpression(call->args[1]);
    if (!lhs_expr.IsValid() || !rhs_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value lhs_value = EnsureValue(lhs_expr, block);
    ir::Value rhs_value = EnsureValue(rhs_expr, block);
    if (!lhs_value.IsValue() || !rhs_value.IsValue() ||
        lhs_value.type != rhs_value.type ||
        !is_f32_or_f32_vector(lhs_value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kMin;
    builtin_inst.result_type = lhs_value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(lhs_value);
    builtin_inst.operands.push_back(rhs_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "clamp") {
    if (call->args.size() != 3u) {
      return ir::ExprResult();
    }

    ir::ExprResult value_expr = LowerExpression(call->args[0]);
    ir::ExprResult low_expr = LowerExpression(call->args[1]);
    ir::ExprResult high_expr = LowerExpression(call->args[2]);
    if (!value_expr.IsValid() || !low_expr.IsValid() || !high_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value value = EnsureValue(value_expr, block);
    ir::Value low = EnsureValue(low_expr, block);
    ir::Value high = EnsureValue(high_expr, block);
    if (!value.IsValue() || !low.IsValue() || !high.IsValue() ||
        value.type != low.type || value.type != high.type ||
        !is_f32_or_f32_vector(value.type)) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kClamp;
    builtin_inst.result_type = value.type;
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(value);
    builtin_inst.operands.push_back(low);
    builtin_inst.operands.push_back(high);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "dot") {
    if (call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult lhs_expr = LowerExpression(call->args[0]);
    ir::ExprResult rhs_expr = LowerExpression(call->args[1]);
    if (!lhs_expr.IsValid() || !rhs_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value lhs_value = EnsureValue(lhs_expr, block);
    ir::Value rhs_value = EnsureValue(rhs_expr, block);
    if (!lhs_value.IsValue() || !rhs_value.IsValue() ||
        lhs_value.type != rhs_value.type ||
        !type_table_->IsVectorType(lhs_value.type) ||
        type_table_->GetComponentType(lhs_value.type) !=
            type_table_->GetF32Type()) {
      return ir::ExprResult();
    }

    ir::Instruction builtin_inst;
    builtin_inst.kind = ir::InstKind::kBuiltinCall;
    builtin_inst.builtin_call = ir::BuiltinCallKind::kDot;
    builtin_inst.result_type = type_table_->GetF32Type();
    builtin_inst.result_id = AllocateSSAId();
    if (builtin_inst.result_id == 0) {
      return ir::ExprResult();
    }
    builtin_inst.operands.push_back(lhs_value);
    builtin_inst.operands.push_back(rhs_value);
    block->instructions.emplace_back(builtin_inst);
    return ir::ExprResult::ValueResult(
        ir::Value::SSA(builtin_inst.result_type, builtin_inst.result_id));
  }

  if (symbol->original_name == "distance") {
    if (call->args.size() != 2u) {
      return ir::ExprResult();
    }

    ir::ExprResult lhs_expr = LowerExpression(call->args[0]);
    ir::ExprResult rhs_expr = LowerExpression(call->args[1]);
    if (!lhs_expr.IsValid() || !rhs_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::Value lhs_value = EnsureValue(lhs_expr, block);
    ir::Value rhs_value = EnsureValue(rhs_expr, block);
    if (!lhs_value.IsValue() || !rhs_value.IsValue() ||
        lhs_value.type != rhs_value.type ||
        !type_table_->IsVectorType(lhs_value.type) ||
        type_table_->GetComponentType(lhs_value.type) !=
            type_table_->GetF32Type()) {
      return ir::ExprResult();
    }

    ir::Instruction sub_inst;
    sub_inst.kind = ir::InstKind::kBinary;
    sub_inst.binary_op = ir::BinaryOpKind::kSubtract;
    sub_inst.result_type = lhs_value.type;
    sub_inst.result_id = AllocateSSAId();
    if (sub_inst.result_id == 0) {
      return ir::ExprResult();
    }
    sub_inst.operands.push_back(lhs_value);
    sub_inst.operands.push_back(rhs_value);
    block->instructions.emplace_back(sub_inst);

    ir::Value delta_value =
        ir::Value::SSA(sub_inst.result_type, sub_inst.result_id);

    ir::Instruction dot_inst;
    dot_inst.kind = ir::InstKind::kBuiltinCall;
    dot_inst.builtin_call = ir::BuiltinCallKind::kDot;
    dot_inst.result_type = type_table_->GetF32Type();
    dot_inst.result_id = AllocateSSAId();
    if (dot_inst.result_id == 0) {
      return ir::ExprResult();
    }
    dot_inst.operands.push_back(delta_value);
    dot_inst.operands.push_back(delta_value);
    block->instructions.emplace_back(dot_inst);

    ir::Instruction sqrt_inst;
    sqrt_inst.kind = ir::InstKind::kBuiltinCall;
    sqrt_inst.builtin_call = ir::BuiltinCallKind::kSqrt;
    sqrt_inst.result_type = type_table_->GetF32Type();
    sqrt_inst.result_id = AllocateSSAId();
    if (sqrt_inst.result_id == 0) {
      return ir::ExprResult();
    }
    sqrt_inst.operands.push_back(
        ir::Value::SSA(dot_inst.result_type, dot_inst.result_id));
    block->instructions.emplace_back(sqrt_inst);

    return ir::ExprResult::ValueResult(
        ir::Value::SSA(sqrt_inst.result_type, sqrt_inst.result_id));
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
