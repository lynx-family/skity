// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "lower/lower_internal.h"

namespace wgx {
namespace lower {

namespace {

bool SupportsSwitchType(ir::TypeId type_id, ir::TypeTable* type_table) {
  return type_table != nullptr && type_table->IsIntegerType(type_id);
}

}  // namespace

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
    case ast::StatementType::kIncDecl:
      return LowerIncrementStatement(
          static_cast<const ast::IncrementDeclStatement*>(statement), block);
    case ast::StatementType::kIf:
      return LowerIfStatement(static_cast<const ast::IfStatement*>(statement),
                              block);
    case ast::StatementType::kSwitch:
      return LowerSwitchStatement(
          static_cast<const ast::SwitchStatement*>(statement), block);
    case ast::StatementType::kLoop:
      return LowerLoopStatement(
          static_cast<const ast::LoopStatement*>(statement), block);
    case ast::StatementType::kForLoop:
      return LowerForLoopStatement(
          static_cast<const ast::ForLoopStatement*>(statement), block);
    case ast::StatementType::kWhileLoop:
      return LowerWhileLoopStatement(
          static_cast<const ast::WhileLoopStatement*>(statement), block);
    case ast::StatementType::kBreak:
      return LowerBreakStatement(block);
    case ast::StatementType::kBreakIf:
      return LowerBreakIfStatement(
          static_cast<const ast::BreakIfStatement*>(statement), block);
    case ast::StatementType::kContinue:
      return LowerContinueStatement(block);
    case ast::StatementType::kCall:
      return LowerCallStatement(
          static_cast<const ast::CallStatement*>(statement), block);
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

bool Lowerer::LowerCallStatement(const ast::CallStatement* call,
                                 ir::Block* block) {
  if (call == nullptr || call->expr == nullptr || block == nullptr) {
    return false;
  }

  const ast::Function* callee = FindResolvedFunction(call->expr);
  if (callee == nullptr) {
    return false;
  }

  ir::ExprResult result = LowerFunctionCallExpression(call->expr, block);
  return GetFunctionReturnType(callee) == ir::kInvalidTypeId ||
         result.IsValid();
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
  const ir::BlockId merge_block_id = ir_function_->AllocateBlockId();
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
  const bool then_terminated =
      CurrentBlock() != nullptr && !CurrentBlock()->instructions.empty() &&
      CurrentBlock()->instructions.back().IsTerminator();
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
    const bool else_terminated =
        CurrentBlock() != nullptr && !CurrentBlock()->instructions.empty() &&
        CurrentBlock()->instructions.back().IsTerminator();
    if (CurrentBlock() != nullptr &&
        (CurrentBlock()->instructions.empty() ||
         !CurrentBlock()->instructions.back().IsTerminator())) {
      ir::Instruction else_jump;
      else_jump.kind = ir::InstKind::kBranch;
      else_jump.target_block = merge_block_id;
      CurrentBlock()->instructions.emplace_back(else_jump);
    }

    if (then_terminated && else_terminated) {
      if (ir_function_->GetBlock(merge_block_id) == nullptr &&
          CreateBlockWithId("if.merge", merge_block_id) ==
              ir::kInvalidBlockId) {
        return false;
      }
      ir::Block* merge_block = ir_function_->GetBlock(merge_block_id);
      if (merge_block == nullptr) {
        return false;
      }
      ir::Instruction unreachable_inst;
      unreachable_inst.kind = ir::InstKind::kUnreachable;
      merge_block->instructions.emplace_back(unreachable_inst);
    }
  }

  if (ir_function_->GetBlock(merge_block_id) == nullptr &&
      CreateBlockWithId("if.merge", merge_block_id) == ir::kInvalidBlockId) {
    return false;
  }

  return SwitchToBlock(merge_block_id);
}

bool Lowerer::LowerSwitchStatement(const ast::SwitchStatement* switch_stmt,
                                   ir::Block* block) {
  if (switch_stmt == nullptr || block == nullptr ||
      switch_stmt->condition == nullptr) {
    return false;
  }
  const ir::BlockId switch_entry_block_id = block->id;

  ir::ExprResult switch_expr = LowerExpression(switch_stmt->condition);
  if (!switch_expr.IsValid()) {
    return false;
  }

  ir::Value switch_value = EnsureValue(switch_expr, block);
  if (!switch_value.IsValue() ||
      !SupportsSwitchType(switch_value.type, type_table_)) {
    return false;
  }

  const ir::BlockId merge_block_id = ir_function_->AllocateBlockId();
  if (merge_block_id == ir::kInvalidBlockId) {
    return false;
  }

  struct SelectorDispatch {
    ast::Expression* selector = nullptr;
    const ast::BlockStatement* body = nullptr;
  };

  std::vector<SelectorDispatch> selector_dispatches;
  const ast::BlockStatement* default_body = nullptr;
  for (size_t i = 0; i < switch_stmt->body.size(); ++i) {
    const ast::CaseStatement* case_stmt = switch_stmt->body[i];
    if (case_stmt == nullptr || case_stmt->body == nullptr) {
      return false;
    }
    for (auto* selector : case_stmt->selectors) {
      if (selector == nullptr) {
        return false;
      }
      if (selector->IsDefault()) {
        default_body = case_stmt->body;
        continue;
      }
      selector_dispatches.push_back(
          SelectorDispatch{selector->expr, case_stmt->body});
    }
  }

  std::vector<ir::BlockId> test_block_ids(selector_dispatches.size(),
                                          ir::kInvalidBlockId);
  std::vector<ir::BlockId> body_block_ids(selector_dispatches.size(),
                                          ir::kInvalidBlockId);
  std::vector<ir::BlockId> selector_merge_block_ids(selector_dispatches.size(),
                                                    ir::kInvalidBlockId);

  for (size_t i = 0; i < selector_dispatches.size(); ++i) {
    test_block_ids[i] =
        i == 0 ? switch_entry_block_id : ir_function_->AllocateBlockId();
    body_block_ids[i] = ir_function_->AllocateBlockId();
    selector_merge_block_ids[i] =
        i == 0 ? merge_block_id : ir_function_->AllocateBlockId();
    if (test_block_ids[i] == ir::kInvalidBlockId ||
        body_block_ids[i] == ir::kInvalidBlockId ||
        selector_merge_block_ids[i] == ir::kInvalidBlockId) {
      return false;
    }
  }

  ir::BlockId default_block_id = merge_block_id;
  if (default_body != nullptr) {
    default_block_id = ir_function_->AllocateBlockId();
    if (default_block_id == ir::kInvalidBlockId) {
      return false;
    }
  }

  for (size_t i = 1; i < test_block_ids.size(); ++i) {
    if (CreateBlockWithId("switch.test." + std::to_string(i),
                          test_block_ids[i]) == ir::kInvalidBlockId) {
      return false;
    }
  }
  for (size_t i = 0; i < body_block_ids.size(); ++i) {
    if (CreateBlockWithId("switch.case." + std::to_string(i),
                          body_block_ids[i]) == ir::kInvalidBlockId) {
      return false;
    }
  }
  if (default_body != nullptr &&
      CreateBlockWithId("switch.default", default_block_id) ==
          ir::kInvalidBlockId) {
    return false;
  }
  for (size_t i = 1; i < selector_merge_block_ids.size(); ++i) {
    if (CreateBlockWithId("switch.merge." + std::to_string(i),
                          selector_merge_block_ids[i]) == ir::kInvalidBlockId) {
      return false;
    }
  }

  if (default_body != nullptr) {
    ir::Block* default_block = ir_function_->GetBlock(default_block_id);
    if (default_block == nullptr || !SwitchToBlock(default_block_id) ||
        !LowerBlockStatement(default_body, default_block)) {
      return false;
    }
  }

  ir::BlockId nested_false_entry = default_block_id;
  ir::BlockId nested_false_exit = default_block_id;

  for (size_t dispatch_index = selector_dispatches.size(); dispatch_index > 0;
       --dispatch_index) {
    const size_t arm_index = dispatch_index - 1;
    const ir::BlockId test_block_id = test_block_ids[arm_index];
    const ir::BlockId body_block_id = body_block_ids[arm_index];
    const ir::BlockId selector_merge_block_id =
        selector_merge_block_ids[arm_index];

    if (!SwitchToBlock(test_block_id)) {
      return false;
    }
    ir::Block* test_block = CurrentBlock();
    if (test_block == nullptr) {
      return false;
    }

    ir::ExprResult selector_expr =
        LowerExpression(selector_dispatches[arm_index].selector);
    if (!selector_expr.IsValid()) {
      return false;
    }

    ir::Value selector_value = EnsureValue(selector_expr, test_block);
    if (!selector_value.IsValue() || selector_value.type != switch_value.type) {
      return false;
    }

    ir::Instruction compare_inst;
    compare_inst.kind = ir::InstKind::kBinary;
    compare_inst.binary_op = ir::BinaryOpKind::kEqual;
    compare_inst.result_type = type_table_->GetBoolType();
    compare_inst.result_id = AllocateSSAId();
    if (compare_inst.result_id == 0) {
      return false;
    }
    compare_inst.operands.push_back(switch_value);
    compare_inst.operands.push_back(selector_value);
    test_block->instructions.emplace_back(compare_inst);

    ir::Instruction branch_inst;
    branch_inst.kind = ir::InstKind::kCondBranch;
    branch_inst.operands.push_back(
        ir::Value::SSA(type_table_->GetBoolType(), compare_inst.result_id));
    branch_inst.true_block = body_block_id;
    branch_inst.false_block = nested_false_entry;
    branch_inst.merge_block = selector_merge_block_id;
    test_block->instructions.emplace_back(branch_inst);

    ir::Block* body_block = ir_function_->GetBlock(body_block_id);
    if (body_block == nullptr || !SwitchToBlock(body_block_id) ||
        !LowerBlockStatement(selector_dispatches[arm_index].body, body_block)) {
      return false;
    }
    if (CurrentBlock() != nullptr &&
        (CurrentBlock()->instructions.empty() ||
         !CurrentBlock()->instructions.back().IsTerminator())) {
      ir::Instruction exit_case;
      exit_case.kind = ir::InstKind::kBranch;
      exit_case.target_block = selector_merge_block_id;
      CurrentBlock()->instructions.emplace_back(exit_case);
    }

    if (nested_false_exit != merge_block_id) {
      ir::Block* false_exit_block = ir_function_->GetBlock(nested_false_exit);
      if (false_exit_block == nullptr || !SwitchToBlock(nested_false_exit)) {
        return false;
      }
      if (CurrentBlock() != nullptr &&
          (CurrentBlock()->instructions.empty() ||
           !CurrentBlock()->instructions.back().IsTerminator())) {
        ir::Instruction exit_nested;
        exit_nested.kind = ir::InstKind::kBranch;
        exit_nested.target_block = selector_merge_block_id;
        CurrentBlock()->instructions.emplace_back(exit_nested);
      }
    }

    nested_false_entry = test_block_id;
    nested_false_exit = selector_merge_block_id;
  }

  if (CreateBlockWithId("switch.merge", merge_block_id) ==
      ir::kInvalidBlockId) {
    return false;
  }
  if (selector_dispatches.empty()) {
    if (!SwitchToBlock(switch_entry_block_id)) {
      return false;
    }
    ir::Instruction enter_target;
    enter_target.kind = ir::InstKind::kBranch;
    enter_target.target_block = default_block_id;
    block->instructions.emplace_back(enter_target);
  }
  if (default_body != nullptr && nested_false_exit == default_block_id) {
    if (!SwitchToBlock(default_block_id)) {
      return false;
    }
    if (CurrentBlock() != nullptr &&
        (CurrentBlock()->instructions.empty() ||
         !CurrentBlock()->instructions.back().IsTerminator())) {
      ir::Instruction exit_default;
      exit_default.kind = ir::InstKind::kBranch;
      exit_default.target_block = merge_block_id;
      CurrentBlock()->instructions.emplace_back(exit_default);
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

bool Lowerer::LowerForLoopStatement(const ast::ForLoopStatement* for_loop,
                                    ir::Block* block) {
  if (for_loop == nullptr || block == nullptr || for_loop->body == nullptr) {
    return false;
  }

  if (for_loop->initializer != nullptr &&
      !LowerStatement(for_loop->initializer, block)) {
    return false;
  }

  ir::Block* current_block = CurrentBlock();
  if (current_block == nullptr) {
    return false;
  }
  if (!current_block->instructions.empty() &&
      current_block->instructions.back().IsTerminator()) {
    return true;
  }

  const ir::BlockId current_block_id = current_block->id;
  const ir::BlockId header_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId condition_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId body_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId continue_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId merge_block_id = ir_function_->AllocateBlockId();
  if (header_block_id == ir::kInvalidBlockId ||
      condition_block_id == ir::kInvalidBlockId ||
      body_block_id == ir::kInvalidBlockId ||
      continue_block_id == ir::kInvalidBlockId ||
      merge_block_id == ir::kInvalidBlockId) {
    return false;
  }

  if (CreateBlockWithId("for.header", header_block_id) == ir::kInvalidBlockId ||
      CreateBlockWithId("for.cond", condition_block_id) ==
          ir::kInvalidBlockId ||
      CreateBlockWithId("for.body", body_block_id) == ir::kInvalidBlockId) {
    return false;
  }

  current_block = ir_function_->GetBlock(current_block_id);
  ir::Block* header_block = ir_function_->GetBlock(header_block_id);
  ir::Block* condition_block = ir_function_->GetBlock(condition_block_id);
  ir::Block* body_block = ir_function_->GetBlock(body_block_id);
  if (current_block == nullptr || header_block == nullptr ||
      condition_block == nullptr || body_block == nullptr) {
    return false;
  }

  ir::Instruction enter_loop;
  enter_loop.kind = ir::InstKind::kBranch;
  enter_loop.target_block = header_block_id;
  current_block->instructions.emplace_back(enter_loop);

  header_block->loop_merge_block = merge_block_id;
  header_block->loop_continue_block = continue_block_id;

  ir::Instruction to_condition;
  to_condition.kind = ir::InstKind::kBranch;
  to_condition.target_block = condition_block_id;
  header_block->instructions.emplace_back(to_condition);

  if (for_loop->condition != nullptr) {
    if (!SwitchToBlock(condition_block_id)) {
      return false;
    }
    ir::ExprResult cond_expr =
        LowerExpression(const_cast<ast::Expression*>(for_loop->condition));
    if (!cond_expr.IsValid()) {
      return false;
    }
    ir::Value cond_value = EnsureValue(cond_expr, condition_block);
    if (!cond_value.IsValue() ||
        cond_value.type != type_table_->GetBoolType()) {
      return false;
    }

    ir::Instruction cond_branch;
    cond_branch.kind = ir::InstKind::kCondBranch;
    cond_branch.operands.push_back(cond_value);
    cond_branch.true_block = body_block_id;
    cond_branch.false_block = merge_block_id;
    condition_block->instructions.emplace_back(cond_branch);
  } else {
    ir::Instruction enter_body;
    enter_body.kind = ir::InstKind::kBranch;
    enter_body.target_block = body_block_id;
    condition_block->instructions.emplace_back(enter_body);
  }

  loop_stack_.push_back(
      LoopContext{header_block_id, continue_block_id, merge_block_id});

  if (!LowerBlockStatement(for_loop->body, body_block)) {
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
    if (CreateBlockWithId("for.continue", continue_block_id) ==
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

  if (for_loop->continuing != nullptr &&
      !LowerStatement(for_loop->continuing, continue_block)) {
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
  if (CreateBlockWithId("for.merge", merge_block_id) == ir::kInvalidBlockId) {
    return false;
  }
  return SwitchToBlock(merge_block_id);
}

bool Lowerer::LowerWhileLoopStatement(const ast::WhileLoopStatement* while_loop,
                                      ir::Block* block) {
  if (while_loop == nullptr || block == nullptr ||
      while_loop->body == nullptr || while_loop->condition == nullptr) {
    return false;
  }

  const ir::BlockId current_block_id = block->id;
  const ir::BlockId header_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId condition_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId body_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId continue_block_id = ir_function_->AllocateBlockId();
  const ir::BlockId merge_block_id = ir_function_->AllocateBlockId();
  if (header_block_id == ir::kInvalidBlockId ||
      condition_block_id == ir::kInvalidBlockId ||
      body_block_id == ir::kInvalidBlockId ||
      continue_block_id == ir::kInvalidBlockId ||
      merge_block_id == ir::kInvalidBlockId) {
    return false;
  }

  if (CreateBlockWithId("while.header", header_block_id) ==
          ir::kInvalidBlockId ||
      CreateBlockWithId("while.cond", condition_block_id) ==
          ir::kInvalidBlockId ||
      CreateBlockWithId("while.body", body_block_id) == ir::kInvalidBlockId) {
    return false;
  }

  ir::Block* current_block = ir_function_->GetBlock(current_block_id);
  ir::Block* header_block = ir_function_->GetBlock(header_block_id);
  ir::Block* condition_block = ir_function_->GetBlock(condition_block_id);
  ir::Block* body_block = ir_function_->GetBlock(body_block_id);
  if (current_block == nullptr || header_block == nullptr ||
      condition_block == nullptr || body_block == nullptr) {
    return false;
  }

  ir::Instruction enter_loop;
  enter_loop.kind = ir::InstKind::kBranch;
  enter_loop.target_block = header_block_id;
  current_block->instructions.emplace_back(enter_loop);

  header_block->loop_merge_block = merge_block_id;
  header_block->loop_continue_block = continue_block_id;

  ir::Instruction to_condition;
  to_condition.kind = ir::InstKind::kBranch;
  to_condition.target_block = condition_block_id;
  header_block->instructions.emplace_back(to_condition);

  if (!SwitchToBlock(condition_block_id)) {
    return false;
  }
  ir::ExprResult cond_expr =
      LowerExpression(const_cast<ast::Expression*>(while_loop->condition));
  if (!cond_expr.IsValid()) {
    return false;
  }
  ir::Value cond_value = EnsureValue(cond_expr, condition_block);
  if (!cond_value.IsValue() || cond_value.type != type_table_->GetBoolType()) {
    return false;
  }

  ir::Instruction cond_branch;
  cond_branch.kind = ir::InstKind::kCondBranch;
  cond_branch.operands.push_back(cond_value);
  cond_branch.true_block = body_block_id;
  cond_branch.false_block = merge_block_id;
  condition_block->instructions.emplace_back(cond_branch);

  loop_stack_.push_back(
      LoopContext{header_block_id, continue_block_id, merge_block_id});

  if (!LowerBlockStatement(while_loop->body, body_block)) {
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

  if (CreateBlockWithId("while.continue", continue_block_id) ==
      ir::kInvalidBlockId) {
    loop_stack_.pop_back();
    return false;
  }
  if (!SwitchToBlock(continue_block_id)) {
    loop_stack_.pop_back();
    return false;
  }

  ir::Instruction loop_back;
  loop_back.kind = ir::InstKind::kBranch;
  loop_back.target_block = header_block_id;
  CurrentBlock()->instructions.emplace_back(loop_back);

  loop_stack_.pop_back();
  if (CreateBlockWithId("while.merge", merge_block_id) == ir::kInvalidBlockId) {
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

bool Lowerer::LowerBreakIfStatement(const ast::BreakIfStatement* break_if,
                                    ir::Block* block) {
  if (break_if == nullptr || block == nullptr ||
      break_if->condition == nullptr || loop_stack_.empty()) {
    return false;
  }

  ir::ExprResult cond_expr =
      LowerExpression(const_cast<ast::Expression*>(break_if->condition));
  if (!cond_expr.IsValid()) {
    return false;
  }

  ir::Value cond_value = EnsureValue(cond_expr, block);
  if (!cond_value.IsValue() || cond_value.type != type_table_->GetBoolType()) {
    return false;
  }

  ir::Instruction inst;
  inst.kind = ir::InstKind::kCondBranch;
  inst.operands.push_back(cond_value);
  inst.true_block = loop_stack_.back().merge_block_id;
  inst.false_block = loop_stack_.back().header_block_id;
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

bool Lowerer::LowerIncrementStatement(const ast::IncrementDeclStatement* inc,
                                      ir::Block* block) {
  if (inc == nullptr || block == nullptr || inc->lhs == nullptr ||
      inc->lhs->GetType() != ast::ExpressionType::kIdentifier) {
    return false;
  }

  auto* ident = static_cast<ast::IdentifierExp*>(inc->lhs);
  if (ident->ident == nullptr) {
    return false;
  }

  const semantic::Symbol* target_symbol = FindResolvedSymbol(ident);
  if (target_symbol == nullptr) {
    return false;
  }

  auto var_info = LookupVar(target_symbol);
  if (var_info.id == 0) {
    var_info = LookupOrRegisterGlobalVar(target_symbol);
    if (var_info.id == 0) {
      return false;
    }
  }

  ir::Value target_var = ir::Value::Variable(var_info.type, var_info.id);
  ir::Instruction load_inst;
  load_inst.kind = ir::InstKind::kLoad;
  load_inst.result_type = var_info.type;
  load_inst.result_id = AllocateSSAId();
  if (load_inst.result_id == 0) {
    return false;
  }
  load_inst.operands.push_back(target_var);
  block->instructions.emplace_back(load_inst);

  ir::Value one = ir::Value::None();
  if (var_info.type == type_table_->GetI32Type()) {
    one = ir::Value::ConstantI32(var_info.type, 1);
  } else if (var_info.type == type_table_->GetU32Type()) {
    one = ir::Value::ConstantU32(var_info.type, 1u);
  } else if (var_info.type == type_table_->GetF32Type()) {
    one = ir::Value::ConstantF32(var_info.type, 1.0f);
  } else {
    return false;
  }

  ir::Instruction binary_inst;
  binary_inst.kind = ir::InstKind::kBinary;
  binary_inst.binary_op =
      inc->increment ? ir::BinaryOpKind::kAdd : ir::BinaryOpKind::kSubtract;
  binary_inst.result_type = var_info.type;
  binary_inst.result_id = AllocateSSAId();
  if (binary_inst.result_id == 0) {
    return false;
  }
  binary_inst.operands.push_back(
      ir::Value::SSA(var_info.type, load_inst.result_id));
  binary_inst.operands.push_back(one);
  block->instructions.emplace_back(binary_inst);

  ir::Instruction store_inst;
  store_inst.kind = ir::InstKind::kStore;
  store_inst.operands.push_back(target_var);
  store_inst.operands.push_back(
      ir::Value::SSA(var_info.type, binary_inst.result_id));
  block->instructions.emplace_back(store_inst);
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
    if (!EmitStore(init_expr, ir::Value::Variable(var_type, var_id), block)) {
      return false;
    }
  }

  return true;
}

bool Lowerer::LowerAssignStatement(const ast::AssignStatement* assign,
                                   ir::Block* block) {
  if (assign == nullptr || block == nullptr || assign->lhs == nullptr ||
      assign->rhs == nullptr) {
    return false;
  }

  if (assign->lhs->GetType() == ast::ExpressionType::kMemberAccessor) {
    auto* member = static_cast<ast::MemberAccessor*>(assign->lhs);
    if (member != nullptr && member->obj != nullptr &&
        member->member != nullptr && !member->member->name.empty()) {
      ir::ExprResult base_expr = LowerExpression(member->obj);
      if (!base_expr.IsValid() || !base_expr.IsAddress()) {
        return false;
      }

      const ir::TypeId base_type = base_expr.GetType();
      if (type_table_->IsVectorType(base_type)) {
        if (assign->op.has_value()) {
          return false;
        }
        std::vector<uint32_t> swizzle_components;
        if (!detail::ParseVectorSwizzle(
                member->member->name,
                type_table_->GetVectorComponentCount(base_type),
                &swizzle_components)) {
          return false;
        }

        ir::ExprResult rhs_expr = LowerExpression(assign->rhs);
        if (!rhs_expr.IsValid()) {
          return false;
        }

        const ir::TypeId component_type =
            type_table_->GetComponentType(base_type);
        if (component_type == ir::kInvalidTypeId) {
          return false;
        }

        if (swizzle_components.size() == 1u) {
          ir::Instruction access_inst;
          access_inst.kind = ir::InstKind::kAccess;
          access_inst.result_id = AllocateSSAId();
          access_inst.result_type = component_type;
          access_inst.access_index = swizzle_components[0];
          access_inst.operands.push_back(base_expr.value);
          if (access_inst.result_id == 0) {
            return false;
          }
          block->instructions.emplace_back(access_inst);
          return EmitStore(
              rhs_expr,
              ir::Value::PointerSSA(component_type, access_inst.result_id),
              block);
        }

        if (detail::HasDuplicateSwizzleComponents(swizzle_components)) {
          return false;
        }

        ir::Value rhs_value = EnsureValue(rhs_expr, block);
        const ir::TypeId rhs_type = type_table_->GetVectorType(
            component_type, static_cast<uint32_t>(swizzle_components.size()));
        if (!rhs_value.IsValue() || rhs_value.type != rhs_type) {
          return false;
        }

        ir::Value base_value = EnsureValue(base_expr, block);
        if (!base_value.IsValue()) {
          return false;
        }

        const uint32_t vector_width =
            type_table_->GetVectorComponentCount(base_type);
        std::vector<int32_t> swizzle_sources(vector_width, -1);
        for (size_t i = 0; i < swizzle_components.size(); ++i) {
          swizzle_sources[swizzle_components[i]] = static_cast<int32_t>(i);
        }

        std::vector<ir::Value> new_components;
        new_components.reserve(vector_width);
        for (uint32_t component_index = 0; component_index < vector_width;
             ++component_index) {
          ir::Instruction extract_inst;
          extract_inst.kind = ir::InstKind::kExtract;
          extract_inst.result_id = AllocateSSAId();
          extract_inst.result_type = component_type;
          extract_inst.operands.push_back(
              swizzle_sources[component_index] >= 0 ? rhs_value : base_value);
          if (swizzle_sources[component_index] >= 0) {
            extract_inst.access_index =
                static_cast<uint32_t>(swizzle_sources[component_index]);
          } else {
            extract_inst.access_index = component_index;
          }
          if (extract_inst.result_id == 0) {
            return false;
          }
          block->instructions.emplace_back(extract_inst);
          new_components.push_back(
              ir::Value::SSA(component_type, extract_inst.result_id));
        }

        ir::Instruction construct_inst;
        construct_inst.kind = ir::InstKind::kConstruct;
        construct_inst.result_id = AllocateSSAId();
        construct_inst.result_type = base_type;
        construct_inst.operands = std::move(new_components);
        if (construct_inst.result_id == 0) {
          return false;
        }
        block->instructions.emplace_back(construct_inst);

        return EmitStore(ir::ExprResult::ValueResult(ir::Value::SSA(
                             base_type, construct_inst.result_id)),
                         base_expr.value, block);
      }
    }
  }

  ir::ExprResult lhs_expr = LowerExpression(assign->lhs);
  if (!lhs_expr.IsValid() || !lhs_expr.IsAddress()) {
    return false;
  }

  ir::ExprResult rhs_expr = LowerExpression(assign->rhs);
  if (!rhs_expr.IsValid()) {
    return false;
  }

  if (assign->op.has_value()) {
    ir::Value lhs_value = EnsureValue(lhs_expr, block);
    if (!lhs_value.IsValue()) {
      return false;
    }

    ir::Value rhs_value = EnsureValue(rhs_expr, block);
    if (!rhs_value.IsValue()) {
      return false;
    }

    if (lhs_value.type != rhs_value.type) {
      return false;
    }

    ir::BinaryOpKind op_kind = ir::BinaryOpKind::kAdd;
    switch (*assign->op) {
      case ast::BinaryOp::kAdd:
        op_kind = ir::BinaryOpKind::kAdd;
        break;
      case ast::BinaryOp::kSubtract:
        op_kind = ir::BinaryOpKind::kSubtract;
        break;
      case ast::BinaryOp::kMultiply:
        op_kind = ir::BinaryOpKind::kMultiply;
        break;
      case ast::BinaryOp::kDivide:
        op_kind = ir::BinaryOpKind::kDivide;
        break;
      case ast::BinaryOp::kModulo:
        op_kind = ir::BinaryOpKind::kModulo;
        break;
      case ast::BinaryOp::kAnd:
        op_kind = ir::BinaryOpKind::kBitwiseAnd;
        break;
      case ast::BinaryOp::kOr:
        op_kind = ir::BinaryOpKind::kBitwiseOr;
        break;
      case ast::BinaryOp::kXor:
        op_kind = ir::BinaryOpKind::kBitwiseXor;
        break;
      case ast::BinaryOp::kShiftLeft:
        op_kind = ir::BinaryOpKind::kShiftLeft;
        break;
      case ast::BinaryOp::kShiftRight:
        op_kind = ir::BinaryOpKind::kShiftRight;
        break;
      default:
        return false;
    }

    uint32_t result_id = AllocateSSAId();
    if (result_id == 0) {
      return false;
    }

    ir::Instruction binary_inst;
    binary_inst.kind = ir::InstKind::kBinary;
    binary_inst.binary_op = op_kind;
    binary_inst.result_type = lhs_value.type;
    binary_inst.result_id = result_id;
    binary_inst.operands.push_back(lhs_value);
    binary_inst.operands.push_back(rhs_value);
    block->instructions.emplace_back(binary_inst);

    return EmitStore(
        ir::ExprResult::ValueResult(ir::Value::SSA(lhs_value.type, result_id)),
        lhs_expr.value, block);
  }

  return EmitStore(rhs_expr, lhs_expr.value, block);
}

}  // namespace lower
}  // namespace wgx
