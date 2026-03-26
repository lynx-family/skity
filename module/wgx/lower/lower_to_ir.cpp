// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "lower/lower_to_ir.h"

#include <array>

#include "wgsl/ast/attribute.h"
#include "wgsl/ast/identifier.h"
#include "wgsl/ast/statement.h"

namespace wgx {
namespace lower {

namespace {

ir::PipelineStage ToIRStage(ast::PipelineStage stage) {
  switch (stage) {
    case ast::PipelineStage::kVertex:
      return ir::PipelineStage::kVertex;
    case ast::PipelineStage::kFragment:
      return ir::PipelineStage::kFragment;
    case ast::PipelineStage::kCompute:
    case ast::PipelineStage::kNone:
      return ir::PipelineStage::kUnknown;
  }

  return ir::PipelineStage::kUnknown;
}

bool IsVec4F32Identifier(const ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return false;
  }

  if (ident->ident->name != "vec4") {
    return false;
  }

  const auto& args = ident->ident->args;
  if (args.size() != 1u || args[0] == nullptr ||
      args[0]->GetType() != ast::ExpressionType::kIdentifier) {
    return false;
  }

  auto* scalar_type = static_cast<ast::IdentifierExp*>(args[0]);
  return scalar_type->ident != nullptr && scalar_type->ident->name == "f32";
}

bool LowerVec4F32Const(ast::Expression* expression,
                       std::array<float, 4>* values) {
  if (expression == nullptr || values == nullptr ||
      expression->GetType() != ast::ExpressionType::kFuncCall) {
    return false;
  }

  auto* call = static_cast<ast::FunctionCallExp*>(expression);
  if (call->ident == nullptr || !IsVec4F32Identifier(call->ident)) {
    return false;
  }

  if (call->args.size() != 4u) {
    return false;
  }

  std::array<float, 4> output = {0.f, 0.f, 0.f, 0.f};
  for (size_t i = 0; i < call->args.size(); ++i) {
    auto* arg = call->args[i];
    if (arg == nullptr || arg->GetType() != ast::ExpressionType::kFloatLiteral) {
      return false;
    }

    output[i] = static_cast<float>(static_cast<ast::FloatLiteralExp*>(arg)->value);
  }

  *values = output;
  return true;
}

bool LowerStatement(const ast::Statement* statement, ir::Block* block) {
  if (statement == nullptr || block == nullptr) {
    return false;
  }

  if (statement->GetType() == ast::StatementType::kReturn) {
    auto* ret = static_cast<const ast::ReturnStatement*>(statement);
    ir::Instruction inst;
    inst.kind = ir::InstKind::kReturn;
    inst.has_return_value = ret->value != nullptr;

    if (ret->value != nullptr) {
      if (!LowerVec4F32Const(ret->value, &inst.const_vec4_f32)) {
        return false;
      }

      inst.return_value_kind = ir::ReturnValueKind::kConstVec4F32;
    }

    block->instructions.emplace_back(inst);
    return true;
  }

  if (statement->GetType() == ast::StatementType::kBlock) {
    auto* nested = static_cast<const ast::BlockStatement*>(statement);
    for (auto* nested_stmt : nested->statements) {
      if (!LowerStatement(nested_stmt, block)) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool HasBuiltinPositionReturn(const ast::Function* function) {
  if (function == nullptr) {
    return false;
  }

  for (auto* attr : function->return_type_attrs) {
    if (attr == nullptr || attr->GetType() != ast::AttributeType::kBuiltin) {
      continue;
    }

    auto* builtin = static_cast<const ast::BuiltinAttribute*>(attr);
    if (builtin->name == "position") {
      return true;
    }
  }

  return false;
}

bool IsReturnTypeVec4F32(const ast::Type& type) {
  if (type.expr == nullptr || type.expr->GetType() != ast::ExpressionType::kIdentifier) {
    return false;
  }

  return IsVec4F32Identifier(type.expr);
}

}  // namespace

std::unique_ptr<ir::Module> LowerToIR(const ast::Module* module,
                                      const ast::Function* entry_point) {
  if (module == nullptr || entry_point == nullptr) {
    return nullptr;
  }

  auto ir_module = std::make_unique<ir::Module>();
  ir_module->entry_point = std::string{entry_point->name->name};
  ir_module->stage = ToIRStage(entry_point->GetPipelineStage());

  if (ir_module->stage == ir::PipelineStage::kUnknown) {
    return nullptr;
  }

  ir::Function ir_function;
  ir_function.name = ir_module->entry_point;
  ir_function.stage = ir_module->stage;
  ir_function.return_builtin_position = HasBuiltinPositionReturn(entry_point);

  if (entry_point->body != nullptr) {
    for (auto* statement : entry_point->body->statements) {
      if (!LowerStatement(statement, &ir_function.entry_block)) {
        return nullptr;
      }
    }
  }

  const bool is_void_return = entry_point->return_type.expr == nullptr;
  const bool is_vec4_f32_return = IsReturnTypeVec4F32(entry_point->return_type);
  const bool has_terminator =
      !ir_function.entry_block.instructions.empty() &&
      ir_function.entry_block.instructions.back().kind == ir::InstKind::kReturn;

  if (is_void_return && !has_terminator) {
    ir::Instruction implicit_return;
    implicit_return.kind = ir::InstKind::kReturn;
    implicit_return.has_return_value = false;
    ir_function.entry_block.instructions.emplace_back(implicit_return);
  } else if (!is_void_return && !is_vec4_f32_return) {
    return nullptr;
  }

  if (has_terminator &&
      ir_function.entry_block.instructions.back().has_return_value &&
      !is_vec4_f32_return) {
    return nullptr;
  }

  if (is_vec4_f32_return && !ir_function.return_builtin_position) {
    return nullptr;
  }

  ir_module->functions.emplace_back(std::move(ir_function));
  return ir_module;
}

}  // namespace lower
}  // namespace wgx
