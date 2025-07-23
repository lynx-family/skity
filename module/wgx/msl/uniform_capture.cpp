// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "msl/uniform_capture.h"

namespace wgx {
namespace msl {

UniformCapture::UniformCapture(Function* scope, ast::Function* func)
    : scope_(scope), func_(func) {}

void UniformCapture::Visit(ast::Expression* expr) {
  switch (expr->GetType()) {
    case ast::ExpressionType::kBoolLiteral:
    case ast::ExpressionType::kIntLiteral:
    case ast::ExpressionType::kFloatLiteral:
      break;
    case ast::ExpressionType::kIdentifier: {
      auto ident = static_cast<ast::IdentifierExp*>(expr);
      ident->ident->Accept(this);
    } break;

    case ast::ExpressionType::kFuncCall: {
      auto func_call = static_cast<ast::FunctionCallExp*>(expr);

      for (auto& arg : func_call->args) {
        arg->Accept(this);
      }

      auto called_func = FindCalledFunction(func_call->ident->ident->name);

      if (called_func && called_func->body) {
        called_func->body->Accept(this);
      }

    } break;

    case ast::ExpressionType::kParenExp: {
      auto paren_exp = static_cast<ast::ParenExp*>(expr);
      for (auto& exp : paren_exp->exps) {
        exp->Accept(this);
      }

    } break;

    case ast::ExpressionType::kUnaryExp: {
      auto unary_exp = static_cast<ast::UnaryExp*>(expr);
      unary_exp->exp->Accept(this);

    } break;

    case ast::ExpressionType::kIndexAccessor: {
      auto index_accessor = static_cast<ast::IndexAccessorExp*>(expr);
      index_accessor->idx->Accept(this);
      index_accessor->obj->Accept(this);
    } break;

    case ast::ExpressionType::kMemberAccessor: {
      auto member_accessor = static_cast<ast::MemberAccessor*>(expr);
      member_accessor->obj->Accept(this);
      member_accessor->member->Accept(this);
    } break;

    case ast::ExpressionType::kBinaryExp: {
      auto binary_exp = static_cast<ast::BinaryExp*>(expr);

      binary_exp->lhs->Accept(this);
      binary_exp->rhs->Accept(this);
    } break;

    default:
      break;
  }
}

void UniformCapture::Visit(ast::Function* function) {}

void UniformCapture::Visit(ast::Identifier* identifier) {
  auto var = scope_->GetGlobalVariable(identifier->name);

  if (var == nullptr) {
    return;
  }

  if (var->GetType() == ast::VariableType::kVar &&
      var->GetAttribute(ast::AttributeType::kGroup)) {
    // This is a uniform variable

    for (auto& uniform : uniforms_) {
      if (uniform->name == var->name) {
        return;
      }
    }

    uniforms_.push_back(static_cast<ast::Var*>(var));
  }
}

void UniformCapture::Visit(ast::Statement* statement) {
  switch (statement->GetType()) {
    case ast::StatementType::kAssign: {
      auto assign = static_cast<ast::AssignStatement*>(statement);

      assign->lhs->Accept(this);
      assign->rhs->Accept(this);
    } break;

    case ast::StatementType::kBlock: {
      auto block = static_cast<ast::BlockStatement*>(statement);

      for (auto& stmt : block->statements) {
        stmt->Accept(this);
      }
    } break;

    case ast::StatementType::kCall: {
      auto call = static_cast<ast::CallStatement*>(statement);
      call->expr->Accept(this);
    } break;

    case ast::StatementType::kIf: {
      auto if_stmt = static_cast<ast::IfStatement*>(statement);

      if_stmt->condition->Accept(this);

      if_stmt->body->Accept(this);

      if (if_stmt->else_stmt) {
        if_stmt->else_stmt->Accept(this);
      }
    } break;

    case ast::StatementType::kLoop: {
      auto loop = static_cast<ast::LoopStatement*>(statement);

      loop->body->Accept(this);

      if (loop->continuing) {
        loop->continuing->Accept(this);
      }
    } break;

    case ast::StatementType::kReturn: {
      auto ret = static_cast<ast::ReturnStatement*>(statement);
      if (ret->value) {
        ret->value->Accept(this);
      }
    } break;

    case ast::StatementType::kSwitch: {
      auto sw = static_cast<ast::SwitchStatement*>(statement);

      sw->condition->Accept(this);

      for (auto& case_stmt : sw->body) {
        case_stmt->Accept(this);
      }
    } break;

    case ast::StatementType::kVarDecl: {
      auto decl = static_cast<ast::VarDeclStatement*>(statement);

      decl->variable->Accept(this);
    } break;

    case ast::StatementType::kIncDecl: {
      auto decl = static_cast<ast::IncrementDeclStatement*>(statement);
      decl->lhs->Accept(this);
    } break;

    case ast::StatementType::kForLoop: {
      auto for_loop = static_cast<ast::ForLoopStatement*>(statement);

      if (for_loop->initializer) {
        for_loop->initializer->Accept(this);
      }

      if (for_loop->condition) {
        for_loop->condition->Accept(this);
      }

      if (for_loop->continuing) {
        for_loop->continuing->Accept(this);
      }

      for_loop->body->Accept(this);

    } break;

    case ast::StatementType::kWhileLoop: {
      auto while_loop = static_cast<ast::WhileLoopStatement*>(statement);

      while_loop->condition->Accept(this);

      while_loop->body->Accept(this);
    } break;

    case ast::StatementType::kBreakIf: {
      auto break_if = static_cast<ast::BreakIfStatement*>(statement);
      break_if->condition->Accept(this);
    } break;

    default:
      break;
  }
}

void UniformCapture::Visit(ast::CaseSelector* case_selector) {}

void UniformCapture::Visit(ast::Variable* variable) {
  if (variable->initializer) {
    variable->initializer->Accept(this);
  }
}

void UniformCapture::Capture() { func_->body->Accept(this); }

ast::Function* UniformCapture::FindCalledFunction(
    const std::string_view& name) {
  for (auto& func : scope_->GetFunctions()) {
    if (func->name->name == name) {
      return func;
    }
  }

  return nullptr;
}

}  // namespace msl
}  // namespace wgx
