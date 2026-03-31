/*
 * Copyright 2021 The Lynx Authors. All rights reserved.
 * Licensed under the Apache License Version 2.0 that can be found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "lower/lower_to_ir.h"

#include <array>
#include <unordered_map>

#include "ir/verifier.h"
#include "ir/value.h"
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

/**
 * Lowerer class: converts AST to IR using unified Value model
 */
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

    /** Create module-level type table (shared across all functions) */
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

#ifndef SKITY_RELEASE
    /** Verify generated IR in debug builds for early error detection */
    auto verify_result = ir::Verify(*ir_function_);
    if (!verify_result.valid) {
      return nullptr;
    }
#endif

    ir_module->functions.emplace_back(std::move(*ir_function));
    return ir_module;
  }

 private:
  /** Lowers the function body statements */
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

  /** Inserts implicit return for void functions if needed */
  bool InsertImplicitReturn() {
    const bool is_void_return = ast_entry_point_->return_type.expr == nullptr;
    const bool is_vec4_f32_return = IsVec4F32Type(ast_entry_point_->return_type);
    const bool has_terminator =
        !ir_function_->entry_block.instructions.empty() &&
        ir_function_->entry_block.instructions.back().kind == ir::InstKind::kReturn;

    if (is_void_return && !has_terminator) {
      ir::Instruction implicit_return;
      implicit_return.kind = ir::InstKind::kReturn;
      ir_function_->entry_block.instructions.emplace_back(implicit_return);
    } else if (!is_void_return && !is_vec4_f32_return) {
      return false;
    }

    if (has_terminator &&
        !ir_function_->entry_block.instructions.back().operands.empty() &&
        !is_vec4_f32_return) {
      return false;
    }

    if (is_vec4_f32_return && !ir_function_->return_builtin_position) {
      return false;
    }

    return true;
  }

  /** Checks if return type has @builtin(position) */
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

  /** Checks if type is vec4<f32> */
  bool IsVec4F32Type(const ast::Type& type) const {
    if (type.expr == nullptr ||
        type.expr->GetType() != ast::ExpressionType::kIdentifier) {
      return false;
    }
    return IsVec4F32Identifier(
        static_cast<const ast::IdentifierExp*>(type.expr));
  }

  /** Converts AST type to IR TypeId */
  ir::TypeId ResolveType(const ast::Type& type) {
    if (type.expr == nullptr) {
      return ir::kInvalidTypeId;
    }

    /** For now, only support vec4<f32> */
    if (type.expr->GetType() == ast::ExpressionType::kIdentifier) {
      auto* ident = static_cast<const ast::IdentifierExp*>(type.expr);
      if (IsVec4F32Identifier(ident)) {
        return type_table_->GetVectorType(type_table_->GetF32Type(), 4);
      }
    }

    return ir::kInvalidTypeId;
  }

  /** Gets the vec4<f32> type */
  ir::TypeId GetVec4F32Type() {
    return type_table_->GetVectorType(type_table_->GetF32Type(), 4);
  }

  /** Lowers a statement to IR */
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

  /**
   * Ensures expr is a value (not an address).
   * If expr is an address (variable reference), emits a load instruction
   * and returns the resulting SSA value.
   */
  ir::Value EnsureValue(const ir::ExprResult& expr, ir::Block* block) {
    if (!expr.IsAddress()) {
      return expr.value;
    }
    /**
     * Emit explicit load: address -> value
     * Note: This creates a kLoad instruction in the IR
     */
    uint32_t load_id = AllocateSSAId();
    ir::Instruction load_inst;
    load_inst.kind = ir::InstKind::kLoad;
    load_inst.result_id = load_id;
    load_inst.result_type = expr.GetType();
    load_inst.operands.push_back(expr.value);
    block->instructions.emplace_back(load_inst);
    return ir::Value::SSA(expr.GetType(), load_id);
  }

  /** Lowers a return statement */
  bool LowerReturnStatement(const ast::ReturnStatement* ret, ir::Block* block) {
    ir::Instruction inst;
    inst.kind = ir::InstKind::kReturn;

    if (ret->value == nullptr) {
      /** Void return */
      block->instructions.emplace_back(inst);
      return true;
    }

    /** Lower the return value expression */
    ir::ExprResult expr_result = LowerExpression(ret->value);
    if (!expr_result.IsValid()) {
      return false;
    }

    /** Ensure we have a value (emit load if needed) */
    ir::Value return_value = EnsureValue(expr_result, block);
    
    /** Verify type */
    if (return_value.type != GetVec4F32Type()) {
      return false;
    }

    /** Set up return instruction with unified Value model */
    if (!return_value.IsValue()) {
      return false;
    }
    inst.operands.push_back(return_value);

    block->instructions.emplace_back(inst);
    return true;
  }

  /** Lowers a block statement */
  bool LowerBlockStatement(const ast::BlockStatement* nested,
                           ir::Block* block) {
    for (auto* nested_stmt : nested->statements) {
      if (!LowerStatement(nested_stmt, block)) {
        return false;
      }
    }
    return true;
  }

  /** Lowers a variable declaration */
  bool LowerVarDecl(const ast::VarDeclStatement* var_decl, ir::Block* block) {
    if (var_decl == nullptr || var_decl->variable == nullptr ||
        var_decl->variable->name == nullptr) {
      return false;
    }

    auto* var = var_decl->variable;
    const std::string var_name = std::string(var->name->name);

    /** Resolve variable type */
    ir::TypeId var_type = ResolveType(var->type);
    if (var_type == ir::kInvalidTypeId) {
      return false;
    }

    const uint32_t var_id = AllocateVarId();
    if (var_id == 0) {
      return false;
    }
    RegisterVar(var_name, var_id);

    /** Emit kVariable instruction */
    ir::Instruction var_inst;
    var_inst.kind = ir::InstKind::kVariable;
    var_inst.var_id = var_id;
    var_inst.result_type = var_type;
    var_inst.var_name = var_name;
    block->instructions.emplace_back(var_inst);

    /** If there's an initializer, emit a store */
    if (var->initializer != nullptr) {
      ir::ExprResult init_expr = LowerExpression(var->initializer);
      if (!init_expr.IsValid()) {
        return false;
      }
      if (!EmitStore(init_expr, var_id, block)) {
        return false;
      }
    }

    return true;
  }

  /** Lowers an assignment statement */
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

    /** Lower the RHS expression */
    ir::ExprResult rhs_expr = LowerExpression(assign->rhs);
    if (!rhs_expr.IsValid()) {
      return false;
    }

    return EmitStore(rhs_expr, var_id, block);
  }

  /**
   * Emits a store instruction.
   * Converts the source expression to a value (with load if needed) and stores
   * it to the target variable.
   */
  bool EmitStore(const ir::ExprResult& source_expr, uint32_t target_var_id,
                 ir::Block* block) {
    if (block == nullptr || target_var_id == 0) {
      return false;
    }

    /** Ensure source is a value (emit load if it's an address) */
    ir::Value source_value = EnsureValue(source_expr, block);
    
    /** Verify type */
    if (source_value.type != GetVec4F32Type()) {
      return false;
    }

    /** Emit store instruction with unified Value model */
    ir::Instruction store_inst;
    store_inst.kind = ir::InstKind::kStore;
    store_inst.operands.push_back(ir::Value::Variable(source_value.type, target_var_id));
    store_inst.operands.push_back(source_value);

    block->instructions.emplace_back(store_inst);
    return true;
  }

  /**
   * Lowers an expression to an ExprResult.
   * Returns an invalid ExprResult on failure.
   */
  ir::ExprResult LowerExpression(ast::Expression* expression) {
    if (expression == nullptr) {
      return ir::ExprResult();
    }

    ir::TypeId vec4_type = GetVec4F32Type();

    /** Try to lower as vec4<f32> constant */
    std::array<float, 4> values;
    if (LowerVec4F32Const(expression, &values)) {
      return ir::ExprResult::ValueResult(
          ir::Value::ConstantVec4F32(vec4_type, values));
    }

    /** Try to lower as variable reference (identifier) */
    if (expression->GetType() == ast::ExpressionType::kIdentifier) {
      return LowerIdentifierExpression(
          static_cast<ast::IdentifierExp*>(expression));
    }

    /** Try to lower as binary expression */
    if (expression->GetType() == ast::ExpressionType::kBinaryExp) {
      return LowerBinaryExpression(
          static_cast<ast::BinaryExp*>(expression));
    }

    return ir::ExprResult();
  }

  /**
   * Lowers a binary expression.
   * Returns the result as an ExprResult with an SSA Value.
   */
  ir::ExprResult LowerBinaryExpression(ast::BinaryExp* binary) {
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

    /** Lower operands */
    ir::ExprResult lhs_expr = LowerExpression(binary->lhs);
    if (!lhs_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::ExprResult rhs_expr = LowerExpression(binary->rhs);
    if (!rhs_expr.IsValid()) {
      return ir::ExprResult();
    }

    ir::TypeId vec4_type = GetVec4F32Type();

    /** Ensure both operands are values (emit loads if needed) */
    ir::Value lhs_value = EnsureValue(lhs_expr, &ir_function_->entry_block);
    ir::Value rhs_value = EnsureValue(rhs_expr, &ir_function_->entry_block);

    /** Type check */
    if (lhs_value.type != vec4_type || rhs_value.type != vec4_type) {
      return ir::ExprResult();
    }

    /** Allocate result SSA id */
    uint32_t result_id = AllocateSSAId();
    if (result_id == 0) {
      return ir::ExprResult();
    }

    /** Emit binary instruction */
    ir::Instruction binary_inst;
    binary_inst.kind = ir::InstKind::kBinary;
    binary_inst.binary_op = op_kind;
    binary_inst.result_type = vec4_type;
    binary_inst.result_id = result_id;
    binary_inst.operands.push_back(lhs_value);
    binary_inst.operands.push_back(rhs_value);

    ir_function_->entry_block.instructions.emplace_back(binary_inst);

    /** Return as SSA Value */
    return ir::ExprResult::ValueResult(ir::Value::SSA(vec4_type, result_id));
  }

  /**
   * Lowers an identifier expression (variable reference).
   * Returns an address ExprResult (lvalue).
   */
  ir::ExprResult LowerIdentifierExpression(ast::IdentifierExp* ident) {
    if (ident->ident == nullptr) {
      return ir::ExprResult();
    }
    const std::string var_name = std::string(ident->ident->name);
    const uint32_t var_id = LookupVar(var_name);
    if (var_id == 0) {
      return ir::ExprResult();
    }

    ir::TypeId vec4_type = GetVec4F32Type();

    /**
     * Variable reference is an address (lvalue), not a value.
     * The caller must use EnsureValue() to convert to a value if needed.
     */
    return ir::ExprResult::AddressResult(
        ir::Value::Variable(vec4_type, var_id));
  }

  /**
   * Extracts vec4<f32> constant from expression.
   * Returns true and fills values on success.
   */
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

  /** Variable management */
  uint32_t AllocateVarId() {
    return ir_function_ ? ir_function_->AllocateVarId() : 0;
  }

  uint32_t AllocateSSAId() {
    return ir_function_ ? ir_function_->AllocateSSAId() : 0;
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
