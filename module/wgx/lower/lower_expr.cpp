// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <string_view>

#include "lower/lower_internal.h"

namespace wgx {
namespace lower {

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
                        uint32_t target_var_id, ir::Block* block) {
  if (block == nullptr || target_var_id == 0) {
    return false;
  }

  ir::Value source_value = EnsureValue(source_expr, block);
  if (!source_value.IsValue()) {
    return false;
  }

  ir::Instruction store_inst;
  store_inst.kind = ir::InstKind::kStore;
  store_inst.operands.push_back(
      ir::Value::Variable(source_value.type, target_var_id));
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

  std::array<float, 4> values = {0.f, 0.f, 0.f, 0.f};
  for (size_t i = 0; i < count; ++i) {
    auto* arg = call->args[i];
    if (arg == nullptr) {
      return ir::ExprResult();
    }

    if (component_type == type_table_->GetF32Type()) {
      if (arg->GetType() != ast::ExpressionType::kFloatLiteral) {
        return ir::ExprResult();
      }
      values[i] =
          static_cast<float>(static_cast<ast::FloatLiteralExp*>(arg)->value);
    } else if (component_type == type_table_->GetI32Type()) {
      if (arg->GetType() != ast::ExpressionType::kIntLiteral) {
        return ir::ExprResult();
      }
      int32_t iv =
          static_cast<int32_t>(static_cast<ast::IntLiteralExp*>(arg)->value);
      std::memcpy(&values[i], &iv, sizeof(iv));
    } else if (component_type == type_table_->GetU32Type()) {
      if (arg->GetType() != ast::ExpressionType::kIntLiteral) {
        return ir::ExprResult();
      }
      uint32_t uv =
          static_cast<uint32_t>(static_cast<ast::IntLiteralExp*>(arg)->value);
      std::memcpy(&values[i], &uv, sizeof(uv));
    } else if (component_type == type_table_->GetBoolType()) {
      if (arg->GetType() != ast::ExpressionType::kBoolLiteral) {
        return ir::ExprResult();
      }
      bool bv = static_cast<ast::BoolLiteralExp*>(arg)->value;
      std::memcpy(&values[i], &bv, sizeof(bv));
    } else {
      return ir::ExprResult();
    }
  }

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
