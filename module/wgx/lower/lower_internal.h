// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/module.h"
#include "semantic/symbol.h"
#include "wgsl/ast/attribute.h"
#include "wgsl/ast/expression.h"
#include "wgsl/ast/function.h"
#include "wgsl/ast/identifier.h"
#include "wgsl/ast/module.h"
#include "wgsl/ast/statement.h"
#include "wgsl/ast/type_decl.h"
#include "wgsl/ast/variable.h"

namespace wgx {
namespace lower {
namespace detail {

// Small parsing/type helpers shared by statement and expression lowering.
ir::PipelineStage ToIRStage(ast::PipelineStage stage);
const ast::IdentifierExp* GetVectorScalarType(const ast::IdentifierExp* ident);
uint32_t GetVectorComponentCount(const ast::IdentifierExp* ident);
const ast::IdentifierExp* GetMatrixScalarType(const ast::IdentifierExp* ident);
uint32_t GetMatrixRowCount(const ast::IdentifierExp* ident);
uint32_t GetMatrixColumnCount(const ast::IdentifierExp* ident);
ir::TypeId ResolveScalarType(const ast::IdentifierExp* ident,
                             ir::TypeTable* type_table);

}  // namespace detail

// Lowerer owns the end-to-end WGSL -> IR lowering state.
// The implementation is intentionally split across core/stmt/expr .cpp files,
// but all mutable lowering state stays centralized here.
class Lowerer {
 public:
  Lowerer(const ast::Module* module, const ast::Function* entry_point,
          const std::unordered_map<const ast::IdentifierExp*,
                                   semantic::Symbol*>& ident_symbols,
          const std::unordered_map<const ast::Identifier*, semantic::Symbol*>&
              decl_symbols);

  std::unique_ptr<ir::Module> Run();

 private:
  struct VarInfo {
    uint32_t id = 0;
    ir::TypeId type = ir::kInvalidTypeId;
  };

  struct LoopContext {
    // break targets merge, continue targets continue_block; header is kept for
    // loop-back edges emitted after the continuing block.
    ir::BlockId header_block_id = ir::kInvalidBlockId;
    ir::BlockId continue_block_id = ir::kInvalidBlockId;
    ir::BlockId merge_block_id = ir::kInvalidBlockId;
  };

  bool LowerFunction(const ast::Function* function, bool is_entry_point);
  bool RegisterFunctionParameters();
  std::vector<ir::InputVariable> ResolveInputVars(
      const ast::Function* function) const;
  bool LowerFunctionBody();
  bool InsertImplicitReturn();
  void ResolveParameterDecorations(const ast::Parameter* param,
                                   ir::FunctionParameter* ir_param) const;
  std::vector<ir::OutputVariable> ResolveOutputVars(
      const ast::Function* function) const;
  ir::TypeId ResolveType(const ast::Type& type);
  bool IsVoidType(const ast::Type& type) const;

  bool LowerStatement(const ast::Statement* statement, ir::Block* block);
  ir::Value EnsureValue(const ir::ExprResult& expr, ir::Block* block);
  bool LowerReturnStatement(const ast::ReturnStatement* ret, ir::Block* block);
  ir::Block* CurrentBlock();
  ir::BlockId CreateBlock(const std::string& name);
  ir::BlockId CreateBlockWithId(const std::string& name, ir::BlockId id);
  bool SwitchToBlock(ir::BlockId id);
  bool LowerBlockStatement(const ast::BlockStatement* nested, ir::Block* block);
  bool LowerIfStatement(const ast::IfStatement* if_stmt, ir::Block* block);
  bool LowerSwitchStatement(const ast::SwitchStatement* switch_stmt,
                            ir::Block* block);
  bool LowerLoopStatement(const ast::LoopStatement* loop_stmt,
                          ir::Block* block);
  bool LowerForLoopStatement(const ast::ForLoopStatement* for_loop,
                             ir::Block* block);
  bool LowerWhileLoopStatement(const ast::WhileLoopStatement* while_loop,
                               ir::Block* block);
  bool LowerBreakStatement(ir::Block* block);
  bool LowerBreakIfStatement(const ast::BreakIfStatement* break_if,
                             ir::Block* block);
  bool LowerContinueStatement(ir::Block* block);
  bool LowerCallStatement(const ast::CallStatement* call, ir::Block* block);
  bool LowerIncrementStatement(const ast::IncrementDeclStatement* inc,
                               ir::Block* block);
  bool LowerVarDecl(const ast::VarDeclStatement* var_decl, ir::Block* block);
  bool LowerAssignStatement(const ast::AssignStatement* assign,
                            ir::Block* block);

  bool EmitStore(const ir::ExprResult& source_expr, const ir::Value& target,
                 ir::Block* block);
  ir::ExprResult LowerExpression(ast::Expression* expression);
  ir::ExprResult LowerConstant(ast::Expression* expression);
  ir::ExprResult LowerVectorConstructor(ast::Expression* expression);
  ir::ExprResult LowerMatrixConstructor(ast::Expression* expression);
  ir::ExprResult LowerArrayConstructor(ast::Expression* expression);
  ir::ExprResult LowerScalarCastConstructor(ast::Expression* expression);
  ir::ExprResult LowerUnaryExpression(ast::UnaryExp* unary);
  ir::ExprResult LowerBinaryExpression(ast::BinaryExp* binary);
  ir::ExprResult LowerIndexAccessorExpression(ast::IndexAccessorExp* index);
  ir::ExprResult LowerMemberAccessorExpression(ast::MemberAccessor* member);
  ir::ExprResult LowerFunctionCallExpression(ast::FunctionCallExp* call,
                                             ir::Block* block);
  ir::ExprResult LowerBuiltinCallExpression(ast::FunctionCallExp* call,
                                            const semantic::Symbol* symbol,
                                            ir::Block* block);
  ir::ExprResult LowerIdentifierExpression(ast::IdentifierExp* ident);

  uint32_t AllocateVarId();
  uint32_t AllocateSSAId();
  bool RegisterVar(const semantic::Symbol* symbol, uint32_t id,
                   ir::TypeId type);
  VarInfo LookupVar(const semantic::Symbol* symbol) const;
  VarInfo LookupOrRegisterGlobalVar(const semantic::Symbol* symbol);

  const semantic::Symbol* FindResolvedSymbol(
      const ast::IdentifierExp* ident) const;
  const semantic::Symbol* FindDeclSymbol(const ast::Identifier* ident) const;
  const ast::Function* FindResolvedFunction(
      const ast::FunctionCallExp* call) const;
  const semantic::Symbol* FindResolvedCallee(
      const ast::FunctionCallExp* call) const;
  bool EnsureFunctionLowered(const ast::Function* function);
  ir::TypeId GetFunctionReturnType(const ast::Function* function);
  const ast::StructDecl* ResolveStructDecl(const ast::Type& type) const;
  const ast::StructDecl* ResolveStructDeclByName(std::string_view name) const;
  void ResolveInterfaceDecorations(
      const std::vector<ast::Attribute*>& attributes,
      ir::InterfaceDecorationKind* decoration_kind,
      uint32_t* decoration_value) const;
  const ir::StructMember* FindStructMember(ir::TypeId struct_type,
                                           std::string_view member_name,
                                           uint32_t* member_index) const;

  const ast::Module* ast_module_;
  const ast::Function* ast_entry_point_;
  const ast::Function* current_ast_function_ = nullptr;
  const std::unordered_map<const ast::IdentifierExp*, semantic::Symbol*>&
      ident_symbols_;
  const std::unordered_map<const ast::Identifier*, semantic::Symbol*>&
      decl_symbols_;
  ir::Module* ir_module_ = nullptr;
  ir::Function* ir_function_ = nullptr;
  ir::TypeTable* type_table_ = nullptr;
  ir::BlockId current_block_id_ = ir::kInvalidBlockId;
  std::unordered_map<const semantic::Symbol*, VarInfo> var_map_;
  std::vector<LoopContext> loop_stack_;
  std::unordered_set<const ast::Function*> lowered_functions_;
  std::unordered_set<const ast::Function*> lowering_functions_;
};

}  // namespace lower
}  // namespace wgx
