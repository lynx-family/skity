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

  if (!LowerFunction(ast_entry_point_, true)) {
    return nullptr;
  }
  return ir_module;
}

bool Lowerer::LowerFunction(const ast::Function* function,
                            bool is_entry_point) {
  if (function == nullptr) {
    return false;
  }
  if (lowered_functions_.count(function) > 0) {
    return true;
  }
  if (lowering_functions_.count(function) > 0) {
    return false;
  }

  lowering_functions_.insert(function);

  auto* saved_ast_function = current_ast_function_;
  auto* saved_ir_function = ir_function_;
  const ir::BlockId saved_block_id = current_block_id_;
  auto saved_var_map = var_map_;
  auto saved_loop_stack = loop_stack_;

  auto ir_function = std::make_unique<ir::Function>();
  ir_function_ = ir_function.get();
  current_ast_function_ = function;
  var_map_.clear();
  loop_stack_.clear();

  ir_function_->name = std::string{function->name->name};
  ir::Block entry_block;
  entry_block.id = ir_function_->AllocateBlockId();
  entry_block.name = "entry";
  ir_function_->entry_block_id = entry_block.id;
  ir_function_->blocks.push_back(std::move(entry_block));
  current_block_id_ = ir_function_->entry_block_id;
  ir_function_->stage =
      is_entry_point ? ir_module_->stage : ir::PipelineStage::kUnknown;
  ir_function_->return_type = ResolveType(function->return_type);
  ir_function_->output_vars = is_entry_point
                                  ? ResolveOutputVars(function)
                                  : std::vector<ir::OutputVariable>{};

  bool ok = RegisterFunctionParameters() && LowerFunctionBody() &&
            InsertImplicitReturn();

#ifndef SKITY_RELEASE
  if (ok) {
    auto verify_result = ir::Verify(*ir_function_);
    ok = verify_result.valid;
  }
#endif

  if (ok) {
    ir_module_->functions.emplace_back(std::move(*ir_function_));
    lowered_functions_.insert(function);
  }

  lowering_functions_.erase(function);
  current_ast_function_ = saved_ast_function;
  ir_function_ = saved_ir_function;
  current_block_id_ = saved_block_id;
  var_map_ = std::move(saved_var_map);
  loop_stack_ = std::move(saved_loop_stack);
  return ok;
}

bool Lowerer::RegisterFunctionParameters() {
  if (current_ast_function_ == nullptr || ir_function_ == nullptr) {
    return false;
  }

  for (auto* param : current_ast_function_->params) {
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

    ir::FunctionParameter ir_param;
    ir_param.name = std::string{param->name->name};
    ir_param.type = param_type;
    ir_param.var_id = var_id;
    ir_function_->parameters.push_back(std::move(ir_param));
  }
  return true;
}

bool Lowerer::LowerFunctionBody() {
  if (current_ast_function_ == nullptr ||
      current_ast_function_->body == nullptr) {
    return true;
  }
  for (auto* statement : current_ast_function_->body->statements) {
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
  if (current_ast_function_ == nullptr) {
    return false;
  }

  const bool is_void_return = IsVoidType(current_ast_function_->return_type);
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

std::vector<ir::OutputVariable> Lowerer::ResolveOutputVars(
    const ast::Function* function) const {
  std::vector<ir::OutputVariable> outputs;

  if (function == nullptr || IsVoidType(function->return_type)) {
    return outputs;
  }

  ir::OutputVariable output;
  output.type = const_cast<Lowerer*>(this)->ResolveType(function->return_type);

  for (auto* attr : function->return_type_attrs) {
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

const ast::Function* Lowerer::FindResolvedFunction(
    const ast::FunctionCallExp* call) const {
  if (call == nullptr || call->ident == nullptr) {
    return nullptr;
  }

  const semantic::Symbol* symbol = FindResolvedSymbol(call->ident);
  if (symbol == nullptr || symbol->kind != semantic::SymbolKind::kFunction ||
      symbol->declaration == nullptr) {
    return nullptr;
  }

  return static_cast<const ast::Function*>(symbol->declaration);
}

bool Lowerer::EnsureFunctionLowered(const ast::Function* function) {
  if (function == nullptr) {
    return false;
  }
  if (lowered_functions_.count(function) > 0) {
    return true;
  }
  if (function == current_ast_function_) {
    return false;
  }
  return LowerFunction(function, false);
}

ir::TypeId Lowerer::GetFunctionReturnType(const ast::Function* function) {
  if (function == nullptr || IsVoidType(function->return_type)) {
    return ir::kInvalidTypeId;
  }
  return ResolveType(function->return_type);
}

}  // namespace lower
}  // namespace wgx
