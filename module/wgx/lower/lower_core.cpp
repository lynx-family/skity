// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ir/verifier.h"
#include "lower/lower_internal.h"

namespace wgx {
namespace lower {
namespace detail {

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

const ast::IdentifierExp* GetVectorScalarType(const ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return nullptr;
  }
  const auto& name = ident->ident->name;
  if (name != "vec2" && name != "vec3" && name != "vec4") {
    return nullptr;
  }
  const auto& args = ident->ident->args;
  if (args.size() != 1u || args[0] == nullptr ||
      args[0]->GetType() != ast::ExpressionType::kIdentifier) {
    return nullptr;
  }
  return static_cast<ast::IdentifierExp*>(args[0]);
}

uint32_t GetVectorComponentCount(const ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return 0;
  }
  const auto& name = ident->ident->name;
  if (name == "vec2") return 2;
  if (name == "vec3") return 3;
  if (name == "vec4") return 4;
  return 0;
}

ir::TypeId ResolveScalarType(const ast::IdentifierExp* ident,
                             ir::TypeTable* type_table) {
  if (ident == nullptr || ident->ident == nullptr) {
    return ir::kInvalidTypeId;
  }
  const auto& name = ident->ident->name;
  if (name == "f32") return type_table->GetF32Type();
  if (name == "i32") return type_table->GetI32Type();
  if (name == "u32") return type_table->GetU32Type();
  if (name == "bool") return type_table->GetBoolType();
  return ir::kInvalidTypeId;
}

}  // namespace detail

Lowerer::Lowerer(const ast::Module* module, const ast::Function* entry_point,
                 const std::unordered_map<const ast::IdentifierExp*,
                                          semantic::Symbol*>& ident_symbols,
                 const std::unordered_map<const ast::Identifier*,
                                          semantic::Symbol*>& decl_symbols)
    : ast_module_(module),
      ast_entry_point_(entry_point),
      ident_symbols_(ident_symbols),
      decl_symbols_(decl_symbols) {}

std::unique_ptr<ir::Module> Lowerer::Run() {
  if (ast_module_ == nullptr || ast_entry_point_ == nullptr) {
    return nullptr;
  }

  auto ir_module = std::make_unique<ir::Module>();
  ir_module_ = ir_module.get();
  ir_module->entry_point = std::string{ast_entry_point_->name->name};
  ir_module->stage = detail::ToIRStage(ast_entry_point_->GetPipelineStage());

  if (ir_module->stage == ir::PipelineStage::kUnknown) {
    return nullptr;
  }

  ir_module->type_table = std::make_unique<ir::TypeTable>();
  type_table_ = ir_module->type_table.get();

  auto ir_function = std::make_unique<ir::Function>();
  ir_function_ = ir_function.get();

  ir_function->name = ir_module->entry_point;
  // All later blocks are created off this entry block; keeping the initial
  // setup here makes the control-flow expansion in lower_stmt.cpp easier to
  // follow.
  ir::Block entry_block;
  entry_block.id = ir_function->AllocateBlockId();
  entry_block.name = "entry";
  ir_function->entry_block_id = entry_block.id;
  ir_function->blocks.push_back(std::move(entry_block));
  current_block_id_ = ir_function->entry_block_id;
  ir_function->stage = ir_module->stage;
  ir_function->return_type = ResolveType(ast_entry_point_->return_type);
  ir_function->output_vars = ResolveOutputVars();

  if (!RegisterFunctionParameters()) {
    return nullptr;
  }
  if (!LowerFunctionBody()) {
    return nullptr;
  }
  if (!InsertImplicitReturn()) {
    return nullptr;
  }

#ifndef SKITY_RELEASE
  // Keep structural verification at the lowering boundary so later backend
  // failures are more likely to be actual capability gaps instead of malformed
  // IR.
  auto verify_result = ir::Verify(*ir_function_);
  if (!verify_result.valid) {
    return nullptr;
  }
#endif

  ir_module->functions.emplace_back(std::move(*ir_function));
  return ir_module;
}

bool Lowerer::RegisterFunctionParameters() {
  for (auto* param : ast_entry_point_->params) {
    if (param == nullptr || param->name == nullptr) {
      continue;
    }

    const semantic::Symbol* param_symbol = FindDeclSymbol(param->name);
    if (param_symbol == nullptr) {
      return false;
    }

    ir::TypeId param_type = ResolveType(param->type);
    if (param_type == ir::kInvalidTypeId) {
      return false;
    }

    const uint32_t var_id = AllocateVarId();
    if (var_id == 0) {
      return false;
    }

    if (!RegisterVar(param_symbol, var_id, param_type)) {
      return false;
    }
  }
  return true;
}

bool Lowerer::LowerFunctionBody() {
  if (ast_entry_point_->body == nullptr) {
    return true;
  }
  for (auto* statement : ast_entry_point_->body->statements) {
    ir::Block* current = CurrentBlock();
    if (current == nullptr) {
      return false;
    }
    if (!current->instructions.empty() &&
        current->instructions.back().IsTerminator()) {
      break;
    }
    if (!LowerStatement(statement, current)) {
      return false;
    }
  }
  return true;
}

bool Lowerer::InsertImplicitReturn() {
  const bool is_void_return = IsVoidType(ast_entry_point_->return_type);
  const bool has_terminator =
      CurrentBlock() != nullptr && !CurrentBlock()->instructions.empty() &&
      CurrentBlock()->instructions.back().IsTerminator();

  if (is_void_return && !has_terminator) {
    ir::Instruction implicit_return;
    implicit_return.kind = ir::InstKind::kReturn;
    CurrentBlock()->instructions.emplace_back(implicit_return);
  }

  if (!is_void_return && !has_terminator) {
    return false;
  }

  return true;
}

std::vector<ir::OutputVariable> Lowerer::ResolveOutputVars() {
  std::vector<ir::OutputVariable> outputs;

  if (IsVoidType(ast_entry_point_->return_type)) {
    return outputs;
  }

  ir::OutputVariable output;
  output.type = ResolveType(ast_entry_point_->return_type);

  for (auto* attr : ast_entry_point_->return_type_attrs) {
    if (attr == nullptr) continue;

    if (attr->GetType() == ast::AttributeType::kBuiltin) {
      auto* builtin = static_cast<const ast::BuiltinAttribute*>(attr);
      if (builtin->name == "position") {
        output.name = "position_output";
        output.SetBuiltin(ir::BuiltinType::kPosition);
        break;
      }
    } else if (attr->GetType() == ast::AttributeType::kLocation) {
      auto* loc = static_cast<const ast::LocationAttribute*>(attr);
      output.name = "location_output_" + std::to_string(loc->index);
      output.decoration_kind = ir::OutputDecorationKind::kLocation;
      output.decoration_value = static_cast<uint32_t>(loc->index);
      break;
    }
  }

  outputs.push_back(std::move(output));
  return outputs;
}

ir::TypeId Lowerer::ResolveType(const ast::Type& type) {
  if (type.expr == nullptr) {
    return ir::kInvalidTypeId;
  }

  if (type.expr->GetType() == ast::ExpressionType::kIdentifier) {
    auto* ident = static_cast<const ast::IdentifierExp*>(type.expr);

    ir::TypeId scalar_type = detail::ResolveScalarType(ident, type_table_);
    if (scalar_type != ir::kInvalidTypeId) {
      return scalar_type;
    }

    const ast::IdentifierExp* scalar_ident = detail::GetVectorScalarType(ident);
    if (scalar_ident != nullptr) {
      ir::TypeId component_type =
          detail::ResolveScalarType(scalar_ident, type_table_);
      if (component_type != ir::kInvalidTypeId) {
        uint32_t count = detail::GetVectorComponentCount(ident);
        if (count >= 2 && count <= 4) {
          return type_table_->GetVectorType(component_type, count);
        }
      }
    }
  }

  return ir::kInvalidTypeId;
}

bool Lowerer::IsVoidType(const ast::Type& type) const {
  return type.expr == nullptr;
}

const semantic::Symbol* Lowerer::FindResolvedSymbol(
    const ast::IdentifierExp* ident) const {
  auto it = ident_symbols_.find(ident);
  return it == ident_symbols_.end() ? nullptr : it->second;
}

const semantic::Symbol* Lowerer::FindDeclSymbol(
    const ast::Identifier* ident) const {
  auto it = decl_symbols_.find(ident);
  return it == decl_symbols_.end() ? nullptr : it->second;
}

}  // namespace lower
}  // namespace wgx
