/*
 * Copyright 2021 The Lynx Authors. All rights reserved.
 * Licensed under the Apache License Version 2.0 that can be found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "lower/lower_to_ir.h"

#include <array>
#include <cstring>
#include <unordered_map>

#include "ir/value.h"
#include "ir/verifier.h"
#include "semantic/symbol.h"
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

/**
 * Extracts the scalar type identifier from a vector type identifier.
 * e.g., "vec4<f32>" -> "f32"
 * Returns nullptr if not a valid vector type.
 */
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

/**
 * Gets the vector component count from identifier name.
 * e.g., "vec4" -> 4
 */
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

/**
 * Resolves scalar type identifier to IR type.
 */
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

}  // namespace

/**
 * Lowerer class: converts AST to IR using unified Value model
 */
class Lowerer {
 public:
  Lowerer(const ast::Module* module, const ast::Function* entry_point,
          const std::unordered_map<const ast::IdentifierExp*,
                                   semantic::Symbol*>& ident_symbols,
          const std::unordered_map<const ast::Identifier*, semantic::Symbol*>&
              decl_symbols)
      : ast_module_(module),
        ast_entry_point_(entry_point),
        ident_symbols_(ident_symbols),
        decl_symbols_(decl_symbols) {}

  std::unique_ptr<ir::Module> Run() {
    if (ast_module_ == nullptr || ast_entry_point_ == nullptr) {
      return nullptr;
    }

    auto ir_module = std::make_unique<ir::Module>();
    ir_module_ = ir_module.get();
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
    ir_function->return_type = ResolveType(ast_entry_point_->return_type);
    ir_function->output_vars = ResolveOutputVars();

    /** Register function parameters before lowering body */
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
  /**
   * Registers function parameters as variables before body lowering.
   * Parameters are treated as pre-declared variables in the function scope.
   */
  bool RegisterFunctionParameters() {
    for (auto* param : ast_entry_point_->params) {
      if (param == nullptr || param->name == nullptr) {
        continue;
      }

      const semantic::Symbol* param_symbol = FindDeclSymbol(param->name);
      if (param_symbol == nullptr) {
        // Parameter not found in semantic binding - this is an error
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
        // Duplicate parameter (should not happen if semantic passed)
        return false;
      }
    }
    return true;
  }

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
    const bool is_void_return = IsVoidType(ast_entry_point_->return_type);
    const bool has_terminator =
        !ir_function_->entry_block.instructions.empty() &&
        ir_function_->entry_block.instructions.back().kind ==
            ir::InstKind::kReturn;

    // Void functions need a return statement
    if (is_void_return && !has_terminator) {
      ir::Instruction implicit_return;
      implicit_return.kind = ir::InstKind::kReturn;
      ir_function_->entry_block.instructions.emplace_back(implicit_return);
    }

    // Non-void functions must have a return statement
    // (this is a structural requirement, not a backend capability check)
    if (!is_void_return && !has_terminator) {
      return false;
    }

    return true;
  }

  /**
   * Resolves output variables from return type attributes.
   * For simple returns: creates a single output variable.
   * For struct returns: would create one per decorated member (future).
   */
  std::vector<ir::OutputVariable> ResolveOutputVars() {
    std::vector<ir::OutputVariable> outputs;

    // Void return - no output variables
    if (IsVoidType(ast_entry_point_->return_type)) {
      return outputs;
    }

    ir::OutputVariable output;
    output.type = ResolveType(ast_entry_point_->return_type);

    // Check for builtin decoration
    for (auto* attr : ast_entry_point_->return_type_attrs) {
      if (attr == nullptr) continue;

      if (attr->GetType() == ast::AttributeType::kBuiltin) {
        auto* builtin = static_cast<const ast::BuiltinAttribute*>(attr);
        if (builtin->name == "position") {
          output.name = "position_output";
          output.SetBuiltin(ir::BuiltinType::kPosition);
          break;
        }
        // Future: handle other builtins (frag_depth, sample_mask, etc.)
      } else if (attr->GetType() == ast::AttributeType::kLocation) {
        auto* loc = static_cast<const ast::LocationAttribute*>(attr);
        output.name = "location_output_" + std::to_string(loc->index);
        output.decoration_kind = ir::OutputDecorationKind::kLocation;
        output.decoration_value = static_cast<uint32_t>(loc->index);
        break;
      }
    }

    // If no decoration found, it's an error for entry point returns
    // (WGSL requires decoration on entry point outputs)
    if (output.decoration_kind == ir::OutputDecorationKind::kNone) {
      // For now, default to no output (will fail backend validation)
      // Future: could generate error here
    }

    outputs.push_back(std::move(output));
    return outputs;
  }

  /** Converts AST type to IR TypeId */
  ir::TypeId ResolveType(const ast::Type& type) {
    if (type.expr == nullptr) {
      return ir::kInvalidTypeId;
    }

    if (type.expr->GetType() == ast::ExpressionType::kIdentifier) {
      auto* ident = static_cast<const ast::IdentifierExp*>(type.expr);

      // Try scalar types first
      ir::TypeId scalar_type = ResolveScalarType(ident, type_table_);
      if (scalar_type != ir::kInvalidTypeId) {
        return scalar_type;
      }

      // Try vector types
      const ast::IdentifierExp* scalar_ident = GetVectorScalarType(ident);
      if (scalar_ident != nullptr) {
        ir::TypeId component_type =
            ResolveScalarType(scalar_ident, type_table_);
        if (component_type != ir::kInvalidTypeId) {
          uint32_t count = GetVectorComponentCount(ident);
          if (count >= 2 && count <= 4) {
            return type_table_->GetVectorType(component_type, count);
          }
        }
      }
    }

    return ir::kInvalidTypeId;
  }

  /** Checks if return type is void */
  bool IsVoidType(const ast::Type& type) const { return type.expr == nullptr; }

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

    /** Verify we have a valid value (structural check, not backend capability)
     */
    if (!return_value.IsValue()) {
      return false;
    }
    inst.operands.push_back(return_value);

    block->instructions.emplace_back(inst);
    return true;
  }

  /** Lowers a block statement. Lexical resolution is provided by semantic
   * binding. */
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

    const semantic::Symbol* decl_symbol = FindDeclSymbol(var->name);
    if (decl_symbol == nullptr) {
      return false;
    }

    /** Resolve variable type */
    ir::TypeId var_type = ResolveType(var->type);
    if (var_type == ir::kInvalidTypeId) {
      return false;
    }

    const uint32_t var_id = AllocateVarId();
    if (var_id == 0) {
      return false;
    }
    if (!RegisterVar(decl_symbol, var_id, var_type)) {
      return false;
    }

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

    const semantic::Symbol* target_symbol = FindResolvedSymbol(ident);
    if (target_symbol == nullptr) {
      return false;
    }

    auto var_info = LookupVar(target_symbol);
    if (var_info.id == 0) {
      return false;
    }

    /** Lower the RHS expression */
    ir::ExprResult rhs_expr = LowerExpression(assign->rhs);
    if (!rhs_expr.IsValid()) {
      return false;
    }

    return EmitStore(rhs_expr, var_info.id, block);
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

    /** Verify we have a valid value (structural check) */
    if (!source_value.IsValue()) {
      return false;
    }

    /** Emit store instruction with unified Value model */
    ir::Instruction store_inst;
    store_inst.kind = ir::InstKind::kStore;
    store_inst.operands.push_back(
        ir::Value::Variable(source_value.type, target_var_id));
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

    /** Try to lower as constant */
    ir::ExprResult const_result = LowerConstant(expression);
    if (const_result.IsValid()) {
      return const_result;
    }

    /** Try to lower as variable reference (identifier) */
    if (expression->GetType() == ast::ExpressionType::kIdentifier) {
      return LowerIdentifierExpression(
          static_cast<ast::IdentifierExp*>(expression));
    }

    /** Try to lower as binary expression */
    if (expression->GetType() == ast::ExpressionType::kBinaryExp) {
      return LowerBinaryExpression(static_cast<ast::BinaryExp*>(expression));
    }

    return ir::ExprResult();
  }

  /**
   * Lowers a constant expression.
   * Returns an invalid ExprResult if not a constant.
   */
  ir::ExprResult LowerConstant(ast::Expression* expression) {
    if (expression == nullptr) {
      return ir::ExprResult();
    }

    // Try float literal
    if (expression->GetType() == ast::ExpressionType::kFloatLiteral) {
      auto* lit = static_cast<ast::FloatLiteralExp*>(expression);
      ir::TypeId f32_type = type_table_->GetF32Type();
      return ir::ExprResult::ValueResult(
          ir::Value::ConstantF32(f32_type, static_cast<float>(lit->value)));
    }

    // Try int literal
    if (expression->GetType() == ast::ExpressionType::kIntLiteral) {
      auto* lit = static_cast<ast::IntLiteralExp*>(expression);
      ir::TypeId i32_type = type_table_->GetI32Type();
      return ir::ExprResult::ValueResult(
          ir::Value::ConstantI32(i32_type, static_cast<int32_t>(lit->value)));
    }

    // Try bool literal
    if (expression->GetType() == ast::ExpressionType::kBoolLiteral) {
      auto* lit = static_cast<ast::BoolLiteralExp*>(expression);
      ir::TypeId bool_type = type_table_->GetBoolType();
      return ir::ExprResult::ValueResult(
          ir::Value::ConstantBool(bool_type, lit->value));
    }

    // Try vector constructor
    if (expression->GetType() == ast::ExpressionType::kFuncCall) {
      return LowerVectorConstructor(expression);
    }

    return ir::ExprResult();
  }

  /**
   * Lowers a vector constructor expression (e.g.,
   * vec4<f32>(1.0, 2.0, 3.0, 4.0)).
   */
  ir::ExprResult LowerVectorConstructor(ast::Expression* expression) {
    if (expression == nullptr ||
        expression->GetType() != ast::ExpressionType::kFuncCall) {
      return ir::ExprResult();
    }

    auto* call = static_cast<ast::FunctionCallExp*>(expression);
    if (call->ident == nullptr) {
      return ir::ExprResult();
    }

    // Check if it's a vector type constructor
    uint32_t count = GetVectorComponentCount(call->ident);
    if (count < 2 || count > 4) {
      return ir::ExprResult();
    }

    const ast::IdentifierExp* scalar_ident = GetVectorScalarType(call->ident);
    if (scalar_ident == nullptr) {
      return ir::ExprResult();
    }

    ir::TypeId component_type = ResolveScalarType(scalar_ident, type_table_);
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

    // Create appropriate constant based on count
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

    /** Ensure both operands are values (emit loads if needed) */
    ir::Value lhs_value = EnsureValue(lhs_expr, &ir_function_->entry_block);
    ir::Value rhs_value = EnsureValue(rhs_expr, &ir_function_->entry_block);

    /** Type check - both operands must have the same type */
    if (lhs_value.type != rhs_value.type) {
      return ir::ExprResult();
    }

    ir::TypeId result_type = lhs_value.type;

    /** Allocate result SSA id */
    uint32_t result_id = AllocateSSAId();
    if (result_id == 0) {
      return ir::ExprResult();
    }

    /** Emit binary instruction */
    ir::Instruction binary_inst;
    binary_inst.kind = ir::InstKind::kBinary;
    binary_inst.binary_op = op_kind;
    binary_inst.result_type = result_type;
    binary_inst.result_id = result_id;
    binary_inst.operands.push_back(lhs_value);
    binary_inst.operands.push_back(rhs_value);

    ir_function_->entry_block.instructions.emplace_back(binary_inst);

    /** Return as SSA Value */
    return ir::ExprResult::ValueResult(ir::Value::SSA(result_type, result_id));
  }

  /**
   * Lowers an identifier expression (variable reference).
   * Returns an address ExprResult (lvalue).
   * Supports local variables, parameters, and global variables.
   */
  ir::ExprResult LowerIdentifierExpression(ast::IdentifierExp* ident) {
    if (ident->ident == nullptr) {
      return ir::ExprResult();
    }
    const semantic::Symbol* symbol = FindResolvedSymbol(ident);
    if (symbol == nullptr) {
      return ir::ExprResult();
    }

    auto var_info = LookupVar(symbol);
    if (var_info.id == 0) {
      // Not a local variable or parameter - try global variable
      var_info = LookupOrRegisterGlobalVar(symbol);
      if (var_info.id == 0) {
        return ir::ExprResult();
      }
    }

    /**
     * Variable reference is an address (lvalue), not a value.
     * The caller must use EnsureValue() to convert to a value if needed.
     */
    return ir::ExprResult::AddressResult(
        ir::Value::Variable(var_info.type, var_info.id));
  }

  struct VarInfo {
    uint32_t id = 0;
    ir::TypeId type = ir::kInvalidTypeId;
  };

  /** Variable management */
  uint32_t AllocateVarId() {
    return ir_function_ ? ir_function_->AllocateVarId() : 0;
  }

  uint32_t AllocateSSAId() {
    return ir_function_ ? ir_function_->AllocateSSAId() : 0;
  }

  bool RegisterVar(const semantic::Symbol* symbol, uint32_t id,
                   ir::TypeId type) {
    if (symbol == nullptr) {
      return false;
    }
    auto [it, inserted] = var_map_.emplace(symbol, VarInfo{id, type});
    return inserted;
  }

  VarInfo LookupVar(const semantic::Symbol* symbol) const {
    if (symbol == nullptr) {
      return VarInfo{};
    }
    auto it = var_map_.find(symbol);
    return it == var_map_.end() ? VarInfo{} : it->second;
  }

  /**
   * Looks up or registers a global variable.
   * Global variables are registered on first reference (lazy registration).
   * If the global variable has a constant initializer, it is stored in
   * ir_module_->global_initializers for the backend to use.
   */
  VarInfo LookupOrRegisterGlobalVar(const semantic::Symbol* symbol) {
    if (symbol == nullptr || symbol->declaration == nullptr) {
      return VarInfo{};
    }

    // Check if this symbol corresponds to a global variable declaration
    const ast::Node* decl = symbol->declaration;
    ast::Variable* global_var = nullptr;

    for (auto* var : ast_module_->global_declarations) {
      if (var == decl) {
        global_var = var;
        break;
      }
    }

    if (global_var == nullptr) {
      // Not a global variable
      return VarInfo{};
    }

    // Check if already registered (e.g., multiple references to same global)
    auto it = var_map_.find(symbol);
    if (it != var_map_.end()) {
      return it->second;
    }

    // First reference to this global - register it
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

    // Handle global variable initializer (must be constant)
    if (global_var->initializer != nullptr) {
      ir::ExprResult init_expr = LowerExpression(global_var->initializer);
      if (init_expr.IsValid() && init_expr.value.IsConstant()) {
        ir_module_->global_initializers[var_id] = init_expr.value;
      }
      // If initializer is not a constant, we silently ignore it.
      // A proper implementation should report an error here.
    }

    // Note: Global variables are not emitted as kVariable instructions
    // They are handled by the backend (SPIR-V emitter) as module-level
    // variables
    return VarInfo{var_id, var_type};
  }

  const semantic::Symbol* FindResolvedSymbol(
      const ast::IdentifierExp* ident) const {
    auto it = ident_symbols_.find(ident);
    return it == ident_symbols_.end() ? nullptr : it->second;
  }

  const semantic::Symbol* FindDeclSymbol(const ast::Identifier* ident) const {
    auto it = decl_symbols_.find(ident);
    return it == decl_symbols_.end() ? nullptr : it->second;
  }

  const ast::Module* ast_module_;
  const ast::Function* ast_entry_point_;
  const std::unordered_map<const ast::IdentifierExp*, semantic::Symbol*>&
      ident_symbols_;
  const std::unordered_map<const ast::Identifier*, semantic::Symbol*>&
      decl_symbols_;
  ir::Module* ir_module_ = nullptr;
  ir::Function* ir_function_ = nullptr;
  ir::TypeTable* type_table_ = nullptr;
  std::unordered_map<const semantic::Symbol*, VarInfo> var_map_;
};

std::unique_ptr<ir::Module> LowerToIR(
    const ast::Module* module, const ast::Function* entry_point,
    const std::unordered_map<const ast::IdentifierExp*, semantic::Symbol*>&
        ident_symbols,
    const std::unordered_map<const ast::Identifier*, semantic::Symbol*>&
        decl_symbols) {
  Lowerer lowerer(module, entry_point, ident_symbols, decl_symbols);
  return lowerer.Run();
}

}  // namespace lower
}  // namespace wgx
