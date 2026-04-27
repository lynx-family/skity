// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstdarg>
#include <cstdio>
#include <string>

#include "ir/verifier.h"
#include "lower/lower_internal.h"

namespace wgx {
namespace lower {
namespace detail {

namespace {

void LogWgxError(const char* fmt, ...) {
#ifndef NDEBUG
  std::fprintf(stderr, "[skity] [ERROR]");
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);
  std::fprintf(stderr, "\n");
#else
  (void)fmt;
#endif
}

const char* GetStatementTypeName(ast::StatementType type) {
  switch (type) {
    case ast::StatementType::kReturn:
      return "return";
    case ast::StatementType::kBlock:
      return "block";
    case ast::StatementType::kVarDecl:
      return "var_decl";
    case ast::StatementType::kAssign:
      return "assign";
    case ast::StatementType::kIncDecl:
      return "increment";
    case ast::StatementType::kIf:
      return "if";
    case ast::StatementType::kSwitch:
      return "switch";
    case ast::StatementType::kLoop:
      return "loop";
    case ast::StatementType::kForLoop:
      return "for";
    case ast::StatementType::kWhileLoop:
      return "while";
    case ast::StatementType::kBreak:
      return "break";
    case ast::StatementType::kBreakIf:
      return "break_if";
    case ast::StatementType::kContinue:
      return "continue";
    case ast::StatementType::kCall:
      return "call";
    case ast::StatementType::kCase:
      return "case";
    case ast::StatementType::kDiscard:
      return "discard";
  }
  return "unknown";
}

std::string GetFunctionName(const ast::Function* function) {
  if (function == nullptr || function->name == nullptr) {
    return "<null>";
  }
  return std::string(function->name->name);
}

}  // namespace

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

const ast::IdentifierExp* GetMatrixScalarType(const ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return nullptr;
  }

  if (GetMatrixRowCount(ident) == 0 || GetMatrixColumnCount(ident) == 0) {
    return nullptr;
  }

  const auto& args = ident->ident->args;
  if (args.size() != 1u || args[0] == nullptr ||
      args[0]->GetType() != ast::ExpressionType::kIdentifier) {
    return nullptr;
  }

  return static_cast<ast::IdentifierExp*>(args[0]);
}

uint32_t GetMatrixRowCount(const ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return 0;
  }

  const auto& name = ident->ident->name;
  if (name.size() != 6u || name[0] != 'm' || name[1] != 'a' || name[2] != 't' ||
      name[4] != 'x') {
    return 0;
  }

  if (name[3] < '2' || name[3] > '4' || name[5] < '2' || name[5] > '4') {
    return 0;
  }

  return static_cast<uint32_t>(name[5] - '0');
}

uint32_t GetMatrixColumnCount(const ast::IdentifierExp* ident) {
  if (ident == nullptr || ident->ident == nullptr) {
    return 0;
  }

  const auto& name = ident->ident->name;
  if (name.size() != 6u || name[0] != 'm' || name[1] != 'a' || name[2] != 't' ||
      name[4] != 'x') {
    return 0;
  }

  if (name[3] < '2' || name[3] > '4' || name[5] < '2' || name[5] > '4') {
    return 0;
  }

  return static_cast<uint32_t>(name[3] - '0');
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
    detail::LogWgxError(
        "WGX lowering failed: unknown pipeline stage for entry '%s'",
        detail::GetFunctionName(ast_entry_point_).c_str());
    return nullptr;
  }

  ir_module->type_table = std::make_unique<ir::TypeTable>();
  type_table_ = ir_module->type_table.get();

  if (!LowerFunction(ast_entry_point_, true)) {
    detail::LogWgxError(
        "WGX lowering failed: failed to lower entry function '%s'",
        detail::GetFunctionName(ast_entry_point_).c_str());
    return nullptr;
  }
  return ir_module;
}

bool Lowerer::LowerFunction(const ast::Function* function,
                            bool is_entry_point) {
  if (function == nullptr) {
    detail::LogWgxError("WGX lowering failed: null function");
    return false;
  }
  if (lowered_functions_.count(function) > 0) {
    return true;
  }
  if (lowering_functions_.count(function) > 0) {
    detail::LogWgxError(
        "WGX lowering failed: recursive lowering detected for function '%s'",
        detail::GetFunctionName(function).c_str());
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

  bool ok = RegisterFunctionParameters();
  if (!ok) {
    detail::LogWgxError(
        "WGX lowering failed: RegisterFunctionParameters failed for '%s'",
        detail::GetFunctionName(function).c_str());
  }
  if (ok) {
    ir_function_->input_vars = is_entry_point
                                   ? ResolveInputVars(function)
                                   : std::vector<ir::InputVariable>{};
    ir_function_->output_vars = is_entry_point
                                    ? ResolveOutputVars(function)
                                    : std::vector<ir::OutputVariable>{};
  }
  if (ok && !LowerFunctionBody()) {
    detail::LogWgxError(
        "WGX lowering failed: LowerFunctionBody failed for '%s'",
        detail::GetFunctionName(function).c_str());
    ok = false;
  }
  if (ok && !InsertImplicitReturn()) {
    detail::LogWgxError(
        "WGX lowering failed: InsertImplicitReturn failed for '%s'",
        detail::GetFunctionName(function).c_str());
    ok = false;
  }

#ifndef SKITY_RELEASE
  if (ok) {
    auto verify_result = ir::Verify(*ir_function_);
    ok = verify_result.valid;
    if (!ok) {
      detail::LogWgxError(
          "WGX lowering failed: IR verify failed for '%s' at block %zu inst "
          "%zu kind %u: %s",
          detail::GetFunctionName(function).c_str(), verify_result.block_index,
          verify_result.instruction_index,
          static_cast<uint32_t>(verify_result.instruction_kind),
          verify_result.error_message.c_str());
    }
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
    ResolveParameterDecorations(param, &ir_param);
    ir_function_->parameters.push_back(std::move(ir_param));
  }
  return true;
}

std::vector<ir::InputVariable> Lowerer::ResolveInputVars(
    const ast::Function* function) const {
  std::vector<ir::InputVariable> inputs;
  if (function == nullptr) {
    return inputs;
  }

  for (auto* param : function->params) {
    if (param == nullptr || param->name == nullptr) {
      continue;
    }

    const semantic::Symbol* param_symbol = FindDeclSymbol(param->name);
    if (param_symbol == nullptr) {
      continue;
    }

    auto var_it = var_map_.find(param_symbol);
    if (var_it == var_map_.end()) {
      continue;
    }

    const ast::StructDecl* struct_decl = ResolveStructDecl(param->type);
    if (struct_decl != nullptr) {
      for (uint32_t i = 0; i < struct_decl->members.size(); ++i) {
        auto* member = struct_decl->members[i];
        if (member == nullptr || member->name == nullptr) {
          continue;
        }

        ir::InputVariable input;
        input.name = std::string{param->name->name} + "." +
                     std::string{member->name->name};
        input.type = const_cast<Lowerer*>(this)->ResolveType(member->type);
        input.target_var_id = var_it->second.id;
        input.member_index = i;
        ResolveInterfaceDecorations(member->attributes, &input.decoration_kind,
                                    &input.decoration_value);
        if (input.type == ir::kInvalidTypeId ||
            input.decoration_kind == ir::InterfaceDecorationKind::kNone) {
          continue;
        }
        inputs.push_back(std::move(input));
      }
      continue;
    }

    ir::InputVariable input;
    input.name = std::string{param->name->name};
    input.type = var_it->second.type;
    input.target_var_id = var_it->second.id;
    ResolveInterfaceDecorations(param->attributes, &input.decoration_kind,
                                &input.decoration_value);
    if (input.decoration_kind != ir::InterfaceDecorationKind::kNone) {
      inputs.push_back(std::move(input));
    }
  }

  return inputs;
}

void Lowerer::ResolveParameterDecorations(
    const ast::Parameter* param, ir::FunctionParameter* ir_param) const {
  (void)param;
  (void)ir_param;
}

bool Lowerer::LowerFunctionBody() {
  if (current_ast_function_ == nullptr ||
      current_ast_function_->body == nullptr) {
    return true;
  }
  for (auto* statement : current_ast_function_->body->statements) {
    ir::Block* current = CurrentBlock();
    if (current == nullptr) {
      detail::LogWgxError(
          "WGX lowering failed: current block is null in function '%s'",
          detail::GetFunctionName(current_ast_function_).c_str());
      return false;
    }
    if (!current->instructions.empty() &&
        current->instructions.back().IsTerminator()) {
      break;
    }
    if (!LowerStatement(statement, current)) {
      detail::LogWgxError(
          "WGX lowering failed: statement %p ('%s') failed in function '%s'",
          static_cast<const void*>(statement),
          detail::GetStatementTypeName(statement->GetType()),
          detail::GetFunctionName(current_ast_function_).c_str());
      return false;
    }
  }
  return true;
}

bool Lowerer::InsertImplicitReturn() {
  if (current_ast_function_ == nullptr) {
    detail::LogWgxError(
        "WGX lowering failed: InsertImplicitReturn has no current function");
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
    detail::LogWgxError(
        "WGX lowering failed: non-void function '%s' ended without terminator",
        detail::GetFunctionName(current_ast_function_).c_str());
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

  const ast::StructDecl* struct_decl = ResolveStructDecl(function->return_type);
  if (struct_decl != nullptr) {
    for (uint32_t i = 0; i < struct_decl->members.size(); ++i) {
      auto* member = struct_decl->members[i];
      if (member == nullptr || member->name == nullptr) {
        continue;
      }

      ir::OutputVariable output;
      output.name = std::string{member->name->name};
      output.type = const_cast<Lowerer*>(this)->ResolveType(member->type);
      output.member_index = i;
      ResolveInterfaceDecorations(member->attributes, &output.decoration_kind,
                                  &output.decoration_value);
      if (output.type == ir::kInvalidTypeId ||
          output.decoration_kind == ir::InterfaceDecorationKind::kNone) {
        continue;
      }
      outputs.push_back(std::move(output));
    }
    return outputs;
  }

  ir::OutputVariable output;
  output.type = const_cast<Lowerer*>(this)->ResolveType(function->return_type);
  ResolveInterfaceDecorations(function->return_type_attrs,
                              &output.decoration_kind,
                              &output.decoration_value);
  if (output.decoration_kind == ir::InterfaceDecorationKind::kBuiltin &&
      output.GetBuiltin() == ir::BuiltinType::kPosition) {
    output.name = "position_output";
  } else if (output.decoration_kind == ir::InterfaceDecorationKind::kLocation) {
    output.name = "location_output_" + std::to_string(output.GetLocation());
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

    if (ident->ident->name == "sampler") {
      return type_table_->GetSamplerType();
    }

    if (ident->ident->name == "texture_2d") {
      const auto& args = ident->ident->args;
      if (args.size() != 1u || args[0] == nullptr ||
          args[0]->GetType() != ast::ExpressionType::kIdentifier) {
        return ir::kInvalidTypeId;
      }

      ir::TypeId sampled_type = detail::ResolveScalarType(
          static_cast<const ast::IdentifierExp*>(args[0]), type_table_);
      if (sampled_type == ir::kInvalidTypeId) {
        return ir::kInvalidTypeId;
      }
      return type_table_->GetTexture2DType(sampled_type);
    }

    if (ident->ident->name == "array") {
      const auto& args = ident->ident->args;
      if (args.size() != 2u || args[0] == nullptr || args[1] == nullptr ||
          args[0]->GetType() != ast::ExpressionType::kIdentifier ||
          args[1]->GetType() != ast::ExpressionType::kIntLiteral) {
        return ir::kInvalidTypeId;
      }

      ast::Type element_type_decl;
      element_type_decl.expr = static_cast<ast::IdentifierExp*>(args[0]);
      ir::TypeId element_type = ResolveType(element_type_decl);
      if (element_type == ir::kInvalidTypeId) {
        return ir::kInvalidTypeId;
      }

      auto* count_lit = static_cast<ast::IntLiteralExp*>(args[1]);
      if (count_lit->value <= 0) {
        return ir::kInvalidTypeId;
      }

      return type_table_->GetArrayType(element_type,
                                       static_cast<uint32_t>(count_lit->value));
    }

    if (const ast::StructDecl* struct_decl = ResolveStructDecl(type);
        struct_decl != nullptr) {
      std::vector<ir::StructMember> members;
      members.reserve(struct_decl->members.size());
      for (auto* member : struct_decl->members) {
        if (member == nullptr || member->name == nullptr) {
          continue;
        }
        ir::TypeId member_type = ResolveType(member->type);
        if (member_type == ir::kInvalidTypeId) {
          return ir::kInvalidTypeId;
        }
        members.push_back(
            ir::StructMember{member_type, std::string{member->name->name}, 0});
      }
      return type_table_->GetStructType(members);
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

    scalar_ident = detail::GetMatrixScalarType(ident);
    if (scalar_ident != nullptr) {
      ir::TypeId component_type =
          detail::ResolveScalarType(scalar_ident, type_table_);
      if (component_type != ir::kInvalidTypeId) {
        uint32_t rows = detail::GetMatrixRowCount(ident);
        uint32_t cols = detail::GetMatrixColumnCount(ident);
        if (rows >= 2 && rows <= 4 && cols >= 2 && cols <= 4) {
          return type_table_->GetMatrixType(component_type, rows, cols);
        }
      }
    }
  }

  return ir::kInvalidTypeId;
}

ir::TypeId Lowerer::CreateBufferCompatibleType(ir::TypeId type,
                                               ir::TypeTable::LayoutRule rule) {
  const ir::Type* resolved_type =
      type_table_ != nullptr ? type_table_->GetType(type) : nullptr;
  if (resolved_type == nullptr) {
    return ir::kInvalidTypeId;
  }

  switch (resolved_type->kind) {
    case ir::TypeKind::kStruct: {
      std::vector<ir::StructMember> members;
      members.reserve(resolved_type->members.size());

      uint32_t offset = 0;
      for (const auto& member : resolved_type->members) {
        ir::TypeId member_type = CreateBufferCompatibleType(member.type, rule);
        if (member_type == ir::kInvalidTypeId) {
          return ir::kInvalidTypeId;
        }

        auto member_layout = type_table_->GetLayoutInfo(member_type, rule);
        uint32_t member_offset =
            ir::TypeTable::AlignOffset(offset, member_layout.alignment);
        members.push_back(
            ir::StructMember{member_type, member.name, member_offset});
        offset = member_offset + member_layout.size;
      }

      return type_table_->GetStructType(members);
    }

    case ir::TypeKind::kArray: {
      ir::TypeId element_type =
          CreateBufferCompatibleType(resolved_type->element_type, rule);
      if (element_type == ir::kInvalidTypeId) {
        return ir::kInvalidTypeId;
      }
      return type_table_->GetArrayType(element_type, resolved_type->count);
    }

    default:
      return type;
  }
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
  const semantic::Symbol* symbol = FindResolvedCallee(call);
  if (symbol == nullptr || symbol->kind != semantic::SymbolKind::kFunction ||
      symbol->declaration == nullptr) {
    return nullptr;
  }

  return static_cast<const ast::Function*>(symbol->declaration);
}

const semantic::Symbol* Lowerer::FindResolvedCallee(
    const ast::FunctionCallExp* call) const {
  if (call == nullptr || call->ident == nullptr) {
    return nullptr;
  }

  return FindResolvedSymbol(call->ident);
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

const ast::StructDecl* Lowerer::ResolveStructDecl(const ast::Type& type) const {
  if (type.expr == nullptr ||
      type.expr->GetType() != ast::ExpressionType::kIdentifier) {
    return nullptr;
  }
  auto* ident = static_cast<const ast::IdentifierExp*>(type.expr);
  if (ident->ident == nullptr) {
    return nullptr;
  }
  return ResolveStructDeclByName(ident->ident->name);
}

const ast::StructDecl* Lowerer::ResolveStructDeclByName(
    std::string_view name) const {
  if (ast_module_ == nullptr) {
    return nullptr;
  }

  auto* type_decl = ast_module_->GetGlobalTypeDecl(name);
  if (type_decl == nullptr) {
    return nullptr;
  }

  if (type_decl->GetType() == ast::TypeDeclType::kStruct) {
    return static_cast<const ast::StructDecl*>(type_decl);
  }

  if (type_decl->GetType() != ast::TypeDeclType::kAlias) {
    return nullptr;
  }

  auto* alias = static_cast<const ast::Alias*>(type_decl);
  return ResolveStructDecl(alias->type);
}

void Lowerer::ResolveInterfaceDecorations(
    const std::vector<ast::Attribute*>& attributes,
    ir::InterfaceDecorationKind* decoration_kind,
    uint32_t* decoration_value) const {
  if (decoration_kind == nullptr || decoration_value == nullptr) {
    return;
  }

  *decoration_kind = ir::InterfaceDecorationKind::kNone;
  *decoration_value = 0;

  for (auto* attr : attributes) {
    if (attr == nullptr) {
      continue;
    }

    if (attr->GetType() == ast::AttributeType::kBuiltin) {
      auto* builtin = static_cast<const ast::BuiltinAttribute*>(attr);
      *decoration_kind = ir::InterfaceDecorationKind::kBuiltin;
      if (builtin->name == "position") {
        *decoration_value = static_cast<uint32_t>(ir::BuiltinType::kPosition);
      } else if (builtin->name == "vertex_index") {
        *decoration_value =
            static_cast<uint32_t>(ir::BuiltinType::kVertexIndex);
      } else if (builtin->name == "instance_index") {
        *decoration_value =
            static_cast<uint32_t>(ir::BuiltinType::kInstanceIndex);
      } else {
        *decoration_kind = ir::InterfaceDecorationKind::kNone;
      }
      if (*decoration_kind != ir::InterfaceDecorationKind::kNone) {
        return;
      }
    } else if (attr->GetType() == ast::AttributeType::kLocation) {
      auto* loc = static_cast<const ast::LocationAttribute*>(attr);
      *decoration_kind = ir::InterfaceDecorationKind::kLocation;
      *decoration_value = static_cast<uint32_t>(loc->index);
      return;
    }
  }
}

const ir::StructMember* Lowerer::FindStructMember(
    ir::TypeId struct_type, std::string_view member_name,
    uint32_t* member_index) const {
  if (member_index != nullptr) {
    *member_index = 0;
  }

  const ir::Type* type =
      type_table_ != nullptr ? type_table_->GetType(struct_type) : nullptr;
  if (type == nullptr || type->kind != ir::TypeKind::kStruct) {
    return nullptr;
  }

  for (uint32_t i = 0; i < type->members.size(); ++i) {
    const auto& member = type->members[i];
    if (member.name == member_name) {
      if (member_index != nullptr) {
        *member_index = i;
      }
      return &member;
    }
  }
  return nullptr;
}

}  // namespace lower
}  // namespace wgx
