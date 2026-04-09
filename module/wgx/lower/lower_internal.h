// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ir/module.h"
#include "semantic/symbol.h"
#include "wgsl/ast/attribute.h"
#include "wgsl/ast/expression.h"
#include "wgsl/ast/function.h"
#include "wgsl/ast/identifier.h"
#include "wgsl/ast/module.h"
#include "wgsl/ast/statement.h"
#include "wgsl/ast/variable.h"

namespace wgx {
namespace lower {
namespace detail {

// Small parsing/type helpers shared by statement and expression lowering.
ir::PipelineStage ToIRStage(ast::PipelineStage stage);
const ast::IdentifierExp* GetVectorScalarType(const ast::IdentifierExp* ident);
uint32_t GetVectorComponentCount(const ast::IdentifierExp* ident);
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

  bool RegisterFunctionParameters();
  bool LowerFunctionBody();
  bool InsertImplicitReturn();
  std::vector<ir::OutputVariable> ResolveOutputVars();
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
  bool LowerIncrementStatement(const ast::IncrementDeclStatement* inc,
                               ir::Block* block);
  bool LowerVarDecl(const ast::VarDeclStatement* var_decl, ir::Block* block);
  bool LowerAssignStatement(const ast::AssignStatement* assign,
                            ir::Block* block);

  bool EmitStore(const ir::ExprResult& source_expr, uint32_t target_var_id,
                 ir::Block* block);
  ir::ExprResult LowerExpression(ast::Expression* expression);
  ir::ExprResult LowerConstant(ast::Expression* expression);
  ir::ExprResult LowerVectorConstructor(ast::Expression* expression);
  ir::ExprResult LowerBinaryExpression(ast::BinaryExp* binary);
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

  const ast::Module* ast_module_;
  const ast::Function* ast_entry_point_;
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
};

}  // namespace lower
}  // namespace wgx
