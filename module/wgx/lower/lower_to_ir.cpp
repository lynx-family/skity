// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "lower/lower_to_ir.h"

#include "wgsl/ast/identifier.h"
#include "wgsl/ast/statement.h"

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

void LowerStatement(const ast::Statement* statement, ir::Block* block) {
  if (statement == nullptr || block == nullptr) {
    return;
  }

  if (statement->GetType() == ast::StatementType::kReturn) {
    auto* ret = static_cast<const ast::ReturnStatement*>(statement);
    ir::Instruction inst;
    inst.kind = ir::InstKind::kReturn;
    inst.has_return_value = ret->value != nullptr;
    block->instructions.emplace_back(inst);
    return;
  }

  if (statement->GetType() == ast::StatementType::kBlock) {
    auto* nested = static_cast<const ast::BlockStatement*>(statement);
    for (auto* nested_stmt : nested->statements) {
      LowerStatement(nested_stmt, block);
    }
  }
}

}  // namespace

std::unique_ptr<ir::Module> LowerToIR(const ast::Module* module,
                                      const ast::Function* entry_point) {
  if (module == nullptr || entry_point == nullptr) {
    return nullptr;
  }

  auto ir_module = std::make_unique<ir::Module>();
  ir_module->entry_point = std::string{entry_point->name->name};
  ir_module->stage = ToIRStage(entry_point->GetPipelineStage());

  if (ir_module->stage == ir::PipelineStage::kUnknown) {
    return nullptr;
  }

  ir::Function ir_function;
  ir_function.name = ir_module->entry_point;
  ir_function.stage = ir_module->stage;

  if (entry_point->body != nullptr) {
    for (auto* statement : entry_point->body->statements) {
      LowerStatement(statement, &ir_function.entry_block);
    }
  }

  ir_module->functions.emplace_back(std::move(ir_function));
  return ir_module;
}

}  // namespace lower
}  // namespace wgx
