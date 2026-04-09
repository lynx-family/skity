// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "lower/lower_internal.h"

namespace wgx {
namespace lower {

bool Lowerer::LowerStatement(const ast::Statement* statement,
                             ir::Block* block) {
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
      return LowerVarDecl(static_cast<const ast::VarDeclStatement*>(statement),
                          block);
    case ast::StatementType::kAssign:
      return LowerAssignStatement(
          static_cast<const ast::AssignStatement*>(statement), block);
    case ast::StatementType::kIf:
      return LowerIfStatement(static_cast<const ast::IfStatement*>(statement),
                              block);
    case ast::StatementType::kLoop:
      return LowerLoopStatement(
          static_cast<const ast::LoopStatement*>(statement), block);
    case ast::StatementType::kBreak:
      return LowerBreakStatement(block);
    case ast::StatementType::kContinue:
      return LowerContinueStatement(block);
    default:
      return false;
  }
}

bool Lowerer::LowerReturnStatement(const ast::ReturnStatement* ret,
                                   ir::Block* block) {
  ir::Instruction inst;
  inst.kind = ir::InstKind::kReturn;

  if (ret->value == nullptr) {
    block->instructions.emplace_back(inst);
    return true;
  }

  ir::ExprResult expr_result = LowerExpression(ret->value);
  if (!expr_result.IsValid()) {
    return false;
  }

  ir::Value return_value = EnsureValue(expr_result, block);
  if (!return_value.IsValue()) {
    return false;
  }
  inst.operands.push_back(return_value);

  block->instructions.emplace_back(inst);
  return true;
}

ir::Block* Lowerer::CurrentBlock() {
  return current_block_id_ == ir::kInvalidBlockId
             ? nullptr
             : ir_function_->GetBlock(current_block_id_);
}

ir::BlockId Lowerer::CreateBlock(const std::string& name) {
  ir::Block block;
  block.id = ir_function_->AllocateBlockId();
  block.name = name;
  const ir::BlockId id = block.id;
  ir_function_->blocks.push_back(std::move(block));
  return id;
}

ir::BlockId Lowerer::CreateBlockWithId(const std::string& name,
                                       ir::BlockId id) {
  if (id == ir::kInvalidBlockId || ir_function_ == nullptr ||
      ir_function_->GetBlock(id) != nullptr) {
    return ir::kInvalidBlockId;
  }
  ir::Block block;
  block.id = id;
  block.name = name;
  ir_function_->blocks.push_back(std::move(block));
  return id;
}

bool Lowerer::SwitchToBlock(ir::BlockId id) {
  if (ir_function_->GetBlock(id) == nullptr) {
    return false;
  }
  current_block_id_ = id;
  return true;
}

bool Lowerer::LowerBlockStatement(const ast::BlockStatement* nested,
                                  ir::Block* block) {
  if (nested == nullptr || block == nullptr) {
    return false;
  }
  if (!SwitchToBlock(block->id)) {
    return false;
  }
  for (auto* nested_stmt : nested->statements) {
    ir::Block* current_block = CurrentBlock();
    if (current_block == nullptr) {
      return false;
    }
    // Once a nested statement emits a terminator, the rest of the source block
    // is unreachable in the current IR model.
    if (!current_block->instructions.empty() &&
        current_block->instructions.back().IsTerminator()) {
      break;
    }
    if (!LowerStatement(nested_stmt, current_block)) {
      return false;
    }
  }
  return true;
}

bool Lowerer::LowerIfStatement(const ast::IfStatement* if_stmt,
                               ir::Block* block) {
  if (if_stmt == nullptr || block == nullptr || if_stmt->condition == nullptr ||
      if_stmt->body == nullptr) {
    return false;
  }

  ir::ExprResult cond_expr = LowerExpression(if_stmt->condition);
  if (!cond_expr.IsValid()) {
    return false;
  }

  ir::Value cond_value = EnsureValue(cond_expr, block);
  if (!cond_value.IsValue() || cond_value.type != type_table_->GetBoolType()) {
    return false;
  }

  const ir::BlockId current_block_id = block->id;

  const ir::BlockId then_block_id = CreateBlock("if.then");
  const ir::BlockId merge_block_id = CreateBlock("if.merge");
  if (then_block_id == ir::kInvalidBlockId ||
      merge_block_id == ir::kInvalidBlockId) {
    return false;
  }

  ir::BlockId else_block_id = merge_block_id;
  if (if_stmt->else_stmt != nullptr) {
    else_block_id = CreateBlock("if.else");
    if (else_block_id == ir::kInvalidBlockId) {
      return false;
    }
  }

  ir::Block* current_block = ir_function_->GetBlock(current_block_id);
  if (current_block == nullptr) {
    return false;
  }

  ir::Instruction branch_inst;
  branch_inst.kind = ir::InstKind::kCondBranch;
  branch_inst.operands.push_back(cond_value);
  branch_inst.true_block = then_block_id;
  branch_inst.false_block = else_block_id;
  branch_inst.merge_block = merge_block_id;
  current_block->instructions.emplace_back(branch_inst);

  ir::Block* then_block = ir_function_->GetBlock(then_block_id);
  if (then_block == nullptr || !SwitchToBlock(then_block_id)) {
    return false;
  }
  if (!LowerBlockStatement(if_stmt->body, then_block)) {
    return false;
  }
  if (CurrentBlock() != nullptr &&
      (CurrentBlock()->instructions.empty() ||
       !CurrentBlock()->instructions.back().IsTerminator())) {
    ir::Instruction then_jump;
    then_jump.kind = ir::InstKind::kBranch;
    then_jump.target_block = merge_block_id;
    CurrentBlock()->instructions.emplace_back(then_jump);
  }

  if (if_stmt->else_stmt != nullptr) {
    ir::Block* else_block = ir_function_->GetBlock(else_block_id);
    if (else_block == nullptr || !SwitchToBlock(else_block_id)) {
      return false;
    }
    if (!LowerStatement(if_stmt->else_stmt, else_block)) {
      return false;
    }
    if (CurrentBlock() != nullptr &&
        (CurrentBlock()->instructions.empty() ||
         !CurrentBlock()->instructions.back().IsTerminator())) {
      ir::Instruction else_jump;
      else_jump.kind = ir::InstKind::kBranch;
      else_jump.target_block = merge_block_id;
      CurrentBlock()->instructions.emplace_back(else_jump);
    }
  }

  return SwitchToBlock(merge_block_id);
}

bool Lowerer::LowerLoopStatement(const ast::LoopStatement* loop_stmt,
                                 ir::Block* block) {
  if (loop_stmt == nullptr || block == nullptr || loop_stmt->body == nullptr) {
    return false;
  }

  const ir::BlockId current_block_id = block->id;
  const ir::BlockId header_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId body_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId continue_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId merge_block_id = ir_function_->AllocateBlockId();
  if (header_block_id == ir::kInvalidBlockId ||
      body_block_id == ir::kInvalidBlockId ||
      continue_block_id == ir::kInvalidBlockId ||
      merge_block_id == ir::kInvalidBlockId) {
    return false;
  }

  if (CreateBlockWithId("loop.header", header_block_id) ==
          ir::kInvalidBlockId ||
      CreateBlockWithId("loop.body", body_block_id) == ir::kInvalidBlockId) {
    return false;
  }

  ir::Block* current_block = ir_function_->GetBlock(current_block_id);
  ir::Block* header_block = ir_function_->GetBlock(header_block_id);
  ir::Block* body_block = ir_function_->GetBlock(body_block_id);
  if (current_block == nullptr || header_block == nullptr ||
      body_block == nullptr) {
    return false;
  }

  ir::Instruction enter_loop;
  enter_loop.kind = ir::InstKind::kBranch;
  enter_loop.target_block = header_block_id;
  current_block->instructions.emplace_back(enter_loop);

  // The header block is where SPIR-V emission attaches OpLoopMerge metadata.
  header_block->loop_merge_block = merge_block_id;
  header_block->loop_continue_block = continue_block_id;

  ir::Instruction enter_body;
  enter_body.kind = ir::InstKind::kBranch;
  enter_body.target_block = body_block_id;
  header_block->instructions.emplace_back(enter_body);

  loop_stack_.push_back(
      LoopContext{header_block_id, continue_block_id, merge_block_id});

  if (!SwitchToBlock(body_block_id)) {
    loop_stack_.pop_back();
    return false;
  }
  if (!LowerBlockStatement(loop_stmt->body, body_block)) {
    loop_stack_.pop_back();
    return false;
  }
  if (CurrentBlock() != nullptr &&
      (CurrentBlock()->instructions.empty() ||
       !CurrentBlock()->instructions.back().IsTerminator())) {
    ir::Instruction to_continue;
    to_continue.kind = ir::InstKind::kBranch;
    to_continue.target_block = continue_block_id;
    CurrentBlock()->instructions.emplace_back(to_continue);
  }

  if (!SwitchToBlock(continue_block_id)) {
    // The continue block is created lazily so it stays after the body in block
    // order; this matches the structured SPIR-V layout expected by spirv-val.
    if (CreateBlockWithId("loop.continue", continue_block_id) ==
        ir::kInvalidBlockId) {
      loop_stack_.pop_back();
      return false;
    }
  }
  ir::Block* continue_block = ir_function_->GetBlock(continue_block_id);
  if (continue_block == nullptr || !SwitchToBlock(continue_block_id)) {
    loop_stack_.pop_back();
    return false;
  }
  if (loop_stmt->continuing != nullptr &&
      !LowerBlockStatement(loop_stmt->continuing, continue_block)) {
    loop_stack_.pop_back();
    return false;
  }
  if (CurrentBlock() != nullptr &&
      (CurrentBlock()->instructions.empty() ||
       !CurrentBlock()->instructions.back().IsTerminator())) {
    ir::Instruction loop_back;
    loop_back.kind = ir::InstKind::kBranch;
    loop_back.target_block = header_block_id;
    CurrentBlock()->instructions.emplace_back(loop_back);
  }

  loop_stack_.pop_back();
  // The merge block is also appended after the continue block for the same
  // dominance/structured-control-flow reason.
  if (CreateBlockWithId("loop.merge", merge_block_id) == ir::kInvalidBlockId) {
    return false;
  }
  return SwitchToBlock(merge_block_id);
}

bool Lowerer::LowerBreakStatement(ir::Block* block) {
  if (block == nullptr || loop_stack_.empty()) {
    return false;
  }
  ir::Instruction inst;
  inst.kind = ir::InstKind::kBranch;
  inst.target_block = loop_stack_.back().merge_block_id;
  block->instructions.emplace_back(inst);
  return true;
}

bool Lowerer::LowerContinueStatement(ir::Block* block) {
  if (block == nullptr || loop_stack_.empty()) {
    return false;
  }
  ir::Instruction inst;
  inst.kind = ir::InstKind::kBranch;
  inst.target_block = loop_stack_.back().continue_block_id;
  block->instructions.emplace_back(inst);
  return true;
}

bool Lowerer::LowerVarDecl(const ast::VarDeclStatement* var_decl,
                           ir::Block* block) {
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

  ir::Instruction var_inst;
  var_inst.kind = ir::InstKind::kVariable;
  var_inst.var_id = var_id;
  var_inst.result_type = var_type;
  var_inst.var_name = var_name;
  block->instructions.emplace_back(var_inst);

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

bool Lowerer::LowerAssignStatement(const ast::AssignStatement* assign,
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

  ir::ExprResult rhs_expr = LowerExpression(assign->rhs);
  if (!rhs_expr.IsValid()) {
    return false;
  }

  return EmitStore(rhs_expr, var_info.id, block);
}

}  // namespace lower
}  // namespace wgx
