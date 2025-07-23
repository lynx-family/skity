// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/ast/statement.h"

#include "wgsl/ast/visitor.h"

namespace wgx {
namespace ast {

void Statement::Accept(AstVisitor *visitor) { visitor->Visit(this); }

BlockStatement::BlockStatement(std::vector<Statement *> statements,
                               std::vector<Attribute *> attributes)
    : statements(std::move(statements)), attributes(std::move(attributes)) {}

StatementType BlockStatement::GetType() const { return StatementType::kBlock; }

AssignStatement::AssignStatement(Expression *lhs, Expression *rhs,
                                 std::optional<BinaryOp> op)
    : lhs(lhs), rhs(rhs), op(op) {}

IncrementDeclStatement::IncrementDeclStatement(Expression *lhs, bool inc)
    : lhs(lhs), increment(inc) {}

StatementType IncrementDeclStatement::GetType() const {
  return StatementType::kIncDecl;
}

IfStatement::IfStatement(Expression *condition, BlockStatement *body,
                         Statement *else_stmt,
                         std::vector<Attribute *> attributes)
    : condition(condition),
      body(body),
      else_stmt(else_stmt),
      attributes(std::move(attributes)) {}

StatementType IfStatement::GetType() const { return StatementType::kIf; }

void CaseSelector::Accept(AstVisitor *visitor) { visitor->Visit(this); }

CaseStatement::CaseStatement(std::vector<CaseSelector *> selectors,
                             BlockStatement *body)
    : selectors(std::move(selectors)), body(body) {}

StatementType CaseStatement::GetType() const { return StatementType::kCase; }

SwitchStatement::SwitchStatement(Expression *condition,
                                 std::vector<CaseStatement *> body,
                                 std::vector<Attribute *> stmt_attrs,
                                 std::vector<Attribute *> body_attrs)
    : condition(condition),
      body(std::move(body)),
      stmt_attrs(std::move(stmt_attrs)),
      body_attrs(std::move(body_attrs)) {}

StatementType SwitchStatement::GetType() const {
  return StatementType::kSwitch;
}

LoopStatement::LoopStatement(BlockStatement *body, BlockStatement *continuing,
                             std::vector<Attribute *> attributes)
    : body(body), continuing(continuing), attributes(std::move(attributes)) {}

StatementType LoopStatement::GetType() const { return StatementType::kLoop; }

ForLoopStatement::ForLoopStatement(Statement *initializer,
                                   Expression *condition, Statement *continuing,
                                   BlockStatement *body,
                                   std::vector<Attribute *> attributes)
    : initializer(initializer),
      condition(condition),
      continuing(continuing),
      body(body),
      attributes(std::move(attributes)) {}

StatementType ForLoopStatement::GetType() const {
  return StatementType::kForLoop;
}

WhileLoopStatement::WhileLoopStatement(wgx::ast::Expression *condition,
                                       wgx::ast::BlockStatement *body,
                                       std::vector<Attribute *> attributes)
    : condition(condition), body(body), attributes(std::move(attributes)) {}

StatementType WhileLoopStatement::GetType() const {
  return StatementType::kWhileLoop;
}

BreakIfStatement::BreakIfStatement(Expression *exp) : condition(exp) {}

StatementType BreakIfStatement::GetType() const {
  return StatementType::kBreakIf;
}

}  // namespace ast
}  // namespace wgx
