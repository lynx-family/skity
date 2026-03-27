// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "lower/lower_to_ir.h"

#include <array>
#include <unordered_map>

#include "wgsl/ast/attribute.h"
#include "wgsl/ast/identifier.h"
#include "wgsl/ast/statement.h"
#include "wgsl/ast/variable.h"

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

}  // namespace

// Lowerer class: converts AST to IR using object-oriented design
class Lowerer {
 public:
  Lowerer(const ast::Module* module, const ast::Function* entry_point)
      : ast_module_(module), ast_entry_point_(entry_point) {}

  std::unique_ptr<ir::Module> Run() {
    if (ast_module_ == nullptr || ast_entry_point_ == nullptr) {
      return nullptr;
    }

    auto ir_module = std::make_unique<ir::Module>();
    ir_module->entry_point = std::string{ast_entry_point_->name->name};
    ir_module->stage = ToIRStage(ast_entry_point_->GetPipelineStage());

    if (ir_module->stage == ir::PipelineStage::kUnknown) {
      return nullptr;
    }

    // Create module-level type table (shared across all functions)
    ir_module->type_table = std::make_unique<ir::TypeTable>();
    type_table_ = ir_module->type_table.get();

    auto ir_function = std::make_unique<ir::Function>();
    ir_function_ = ir_function.get();

    ir_function->name = ir_module->entry_point;
    ir_function->stage = ir_module->stage;
    ir_function->return_builtin_position = HasBuiltinPositionReturn();

    if (!LowerFunctionBody()) {
      return nullptr;
    }

    if (!InsertImplicitReturn()) {
      return nullptr;
    }

    ir_module->functions.emplace_back(std::move(*ir_function));
    return ir_module;
  }

 private:
  // Lowers the function body statements
  bool LowerFunctionBody() {
    if (ast_entry_point_->body == nullptr) {
      return true;
    }
    for (auto* statement : ast_entry_point_->body->statements) {
      if (!LowerStatement(statement, &ir_function_->entry_block)) {
        return false;
      }
    }
    return true;
  }

  // Inserts implicit return for void functions if needed
  bool InsertImplicitReturn() {
    const bool is_void_return = ast_entry_point_->return_type.expr == nullptr;
    const bool is_vec4_f32_return = IsVec4F32Type(ast_entry_point_->return_type);
    const bool has_terminator =
        !ir_function_->entry_block.instructions.empty() &&
        ir_function_->entry_block.instructions.back().kind == ir::InstKind::kReturn;

    if (is_void_return && !has_terminator) {
      ir::Instruction implicit_return;
      implicit_return.kind = ir::InstKind::kReturn;
      implicit_return.has_return_value = false;
      ir_function_->entry_block.instructions.emplace_back(implicit_return);
    } else if (!is_void_return && !is_vec4_f32_return) {
      return false;
    }

    if (has_terminator &&
        ir_function_->entry_block.instructions.back().has_return_value &&
        !is_vec4_f32_return) {
      return false;
    }

    if (is_vec4_f32_return && !ir_function_->return_builtin_position) {
      return false;
    }

    return true;
  }

  // Checks if return type has @builtin(position)
  bool HasBuiltinPositionReturn() const {
    for (auto* attr : ast_entry_point_->return_type_attrs) {
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

  // Checks if type is vec4<f32>
  bool IsVec4F32Type(const ast::Type& type) const {
    if (type.expr == nullptr ||
        type.expr->GetType() != ast::ExpressionType::kIdentifier) {
      return false;
    }
    return IsVec4F32Identifier(
        static_cast<const ast::IdentifierExp*>(type.expr));
  }

  // Converts AST type to IR TypeId
  ir::TypeId ResolveType(const ast::Type& type) {
    if (type.expr == nullptr) {
      return ir::kInvalidTypeId;
    }

    // For now, only support vec4<f32>
    if (type.expr->GetType() == ast::ExpressionType::kIdentifier) {
      auto* ident = static_cast<const ast::IdentifierExp*>(type.expr);
      if (IsVec4F32Identifier(ident)) {
        return type_table_->GetVectorType(type_table_->GetF32Type(), 4);
      }
    }

    return ir::kInvalidTypeId;
  }

  // Lowers a statement to IR
  bool LowerStatement(const ast::Statement* statement, ir::Block* block) {
    if (statement == nullptr || block == nullptr) {
      return false;
    }

    switch (statement->GetType()) {
      case ast::StatementType::kReturn:
        return LowerReturnStatement(
            static_cast<const ast::ReturnStatement*>(statement), block);
      case ast::StatementType::kBlock:
        return LowerBlockStatement(
            static_cast<const ast::BlockStatement*>(statement), block);
      case ast::StatementType::kVarDecl:
        return LowerVarDecl(
            static_cast<const ast::VarDeclStatement*>(statement), block);
      case ast::StatementType::kAssign:
        return LowerAssignStatement(
            static_cast<const ast::AssignStatement*>(statement), block);
      default:
        return false;
    }
  }

  // Lowers a return statement
  bool LowerReturnStatement(const ast::ReturnStatement* ret, ir::Block* block) {
    ir::Instruction inst;
    inst.kind = ir::InstKind::kReturn;
    inst.has_return_value = ret->value != nullptr;

    if (ret->value != nullptr) {
      ir::Instruction value_inst;
      if (!LowerExpression(ret->value, &value_inst)) {
        return false;
      }

      ir::TypeId vec4_type = type_table_->GetVectorType(
          type_table_->GetF32Type(), 4);
      if (value_inst.result_type != vec4_type) {
        return false;
      }

      if (value_inst.var_id != 0) {
        // Variable reference - need to load
        inst.return_value_kind = ir::ReturnValueKind::kVariableRef;
        inst.var_id = value_inst.var_id;
      } else {
        // Constant
        inst.const_vec4_f32 = value_inst.const_vec4_f32;
        inst.return_value_kind = ir::ReturnValueKind::kConstVec4F32;
      }
    }

    block->instructions.emplace_back(inst);
    return true;
  }

  // Lowers a block statement
  bool LowerBlockStatement(const ast::BlockStatement* nested,
                           ir::Block* block) {
    for (auto* nested_stmt : nested->statements) {
      if (!LowerStatement(nested_stmt, block)) {
        return false;
      }
    }
    return true;
  }

  // Lowers a variable declaration
  bool LowerVarDecl(const ast::VarDeclStatement* var_decl, ir::Block* block) {
    if (var_decl == nullptr || var_decl->variable == nullptr ||
        var_decl->variable->name == nullptr) {
      return false;
    }

    auto* var = var_decl->variable;
    const std::string var_name = std::string(var->name->name);

    // Resolve variable type
    ir::TypeId var_type = ResolveType(var->type);
    if (var_type == ir::kInvalidTypeId) {
      return false;
    }

    const uint32_t var_id = AllocateVarId();
    if (var_id == 0) {
      return false;
    }
    RegisterVar(var_name, var_id);

    // Emit kVariable instruction
    ir::Instruction var_inst;
    var_inst.kind = ir::InstKind::kVariable;
    var_inst.result_id = var_id;
    var_inst.result_type = var_type;
    var_inst.var_name = var_name;
    block->instructions.emplace_back(var_inst);

    // If there's an initializer, emit a store
    if (var->initializer != nullptr) {
      if (!LowerVarInitializer(var->initializer, var_id, block)) {
        return false;
      }
    }

    return true;
  }

  // Lowers a variable initializer expression
  bool LowerVarInitializer(ast::Expression* initializer, uint32_t var_id,
                           ir::Block* block) {
    ir::Instruction init_inst;
    if (!LowerExpression(initializer, &init_inst)) {
      return false;
    }
    return LowerStoreValue(init_inst, var_id, block);
  }

  // Lowers an assignment statement.
  bool LowerAssignStatement(const ast::AssignStatement* assign,
                            ir::Block* block) {
    if (assign == nullptr || block == nullptr || assign->lhs == nullptr ||
        assign->rhs == nullptr || assign->op.has_value()) {
      return false;
    }

    if (assign->lhs->GetType() != ast::ExpressionType::kIdentifier) {
      return false;
    }

    auto* ident = static_cast<ast::IdentifierExp*>(assign->lhs);
    if (ident->ident == nullptr) {
      return false;
    }

    const uint32_t var_id = LookupVar(std::string(ident->ident->name));
    if (var_id == 0) {
      return false;
    }

    ir::Instruction value_inst;
    if (!LowerExpression(assign->rhs, &value_inst)) {
      return false;
    }

    return LowerStoreValue(value_inst, var_id, block);
  }

  bool LowerStoreValue(const ir::Instruction& value_inst, uint32_t var_id,
                       ir::Block* block) {
    if (block == nullptr || var_id == 0) {
      return false;
    }

    ir::TypeId vec4_type = type_table_->GetVectorType(
        type_table_->GetF32Type(), 4);
    if (value_inst.result_type != vec4_type) {
      return false;
    }

    ir::Instruction store_inst;
    store_inst.kind = ir::InstKind::kStore;
    store_inst.operands.push_back(ir::Operand::Id(var_id));

    if (value_inst.var_id != 0) {
      store_inst.operands.push_back(ir::Operand::Id(value_inst.var_id));
    } else {
      for (size_t i = 0; i < 4; ++i) {
        store_inst.operands.push_back(
            ir::Operand::ConstF32(value_inst.const_vec4_f32[i]));
      }
    }

    block->instructions.emplace_back(store_inst);
    return true;
  }

  // Lowers an expression to an IR instruction
  bool LowerExpression(ast::Expression* expression, ir::Instruction* out_inst) {
    if (expression == nullptr || out_inst == nullptr) {
      return false;
    }

    // Try to lower as vec4<f32> constant
    std::array<float, 4> values;
    if (LowerVec4F32Const(expression, &values)) {
      out_inst->result_type = type_table_->GetVectorType(
          type_table_->GetF32Type(), 4);
      out_inst->const_vec4_f32 = values;
      return true;
    }

    // Try to lower as variable reference (identifier)
    if (expression->GetType() == ast::ExpressionType::kIdentifier) {
      return LowerIdentifierExpression(
          static_cast<ast::IdentifierExp*>(expression), out_inst);
    }

    return false;
  }

  // Lowers an identifier expression (variable reference)
  bool LowerIdentifierExpression(ast::IdentifierExp* ident,
                                 ir::Instruction* out_inst) {
    if (ident->ident == nullptr) {
      return false;
    }
    const std::string var_name = std::string(ident->ident->name);
    const uint32_t var_id = LookupVar(var_name);
    if (var_id == 0) {
      return false;
    }

    out_inst->result_type = type_table_->GetVectorType(
        type_table_->GetF32Type(), 4);
    out_inst->var_id = var_id;
    return true;
  }

  // Extracts vec4<f32> constant from expression
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
      if (arg == nullptr ||
          arg->GetType() != ast::ExpressionType::kFloatLiteral) {
        return false;
      }
      output[i] =
          static_cast<float>(static_cast<ast::FloatLiteralExp*>(arg)->value);
    }

    *values = output;
    return true;
  }

  // Variable management
  uint32_t AllocateVarId() {
    return ir_function_ ? ir_function_->AllocateVarId() : 0;
  }

  void RegisterVar(const std::string& name, uint32_t id) {
    var_ids_[name] = id;
  }

  uint32_t LookupVar(const std::string& name) const {
    auto it = var_ids_.find(name);
    return (it != var_ids_.end()) ? it->second : 0;
  }

 private:
  const ast::Module* ast_module_;
  const ast::Function* ast_entry_point_;
  ir::Function* ir_function_ = nullptr;
  ir::TypeTable* type_table_ = nullptr;
  std::unordered_map<std::string, uint32_t> var_ids_;
};

std::unique_ptr<ir::Module> LowerToIR(const ast::Module* module,
                                      const ast::Function* entry_point) {
  Lowerer lowerer(module, entry_point);
  return lowerer.Run();
}

}  // namespace lower
}  // namespace wgx
