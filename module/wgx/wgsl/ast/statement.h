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

#pragma once

#include <optional>
#include <vector>

#include "wgsl/ast/expression.h"
#include "wgsl/ast/node.h"

namespace wgx {
namespace ast {

struct Attribute;
struct FunctionCallExp;
struct Variable;

enum class StatementType {
  kAssign,
  kBlock,
  kBreak,
  kCase,
  kCall,
  kContinue,
  kDiscard,
  kIf,
  kLoop,
  kReturn,
  kSwitch,
  kVarDecl,
  kIncDecl,
  kForLoop,
  kWhileLoop,
  kBreakIf,
};

struct Statement : public Node {
  ~Statement() override = default;

  virtual StatementType GetType() const = 0;

  void Accept(AstVisitor* visitor) override;
};

struct BlockStatement : public Statement {
  BlockStatement(std::vector<Statement*> statements,
                 std::vector<Attribute*> attributes);

  ~BlockStatement() override = default;

  StatementType GetType() const override;

  std::vector<Statement*> statements;
  std::vector<Attribute*> attributes;
};

struct ReturnStatement : public Statement {
  ReturnStatement() : value(nullptr) {}

  explicit ReturnStatement(Expression* value) : value(value) {}

  ~ReturnStatement() override = default;

  StatementType GetType() const override { return StatementType::kReturn; }

  Expression* value;
};

struct CallStatement : public Statement {
  explicit CallStatement(FunctionCallExp* expr) : expr(expr) {}
  ~CallStatement() override = default;

  StatementType GetType() const override { return StatementType::kCall; }

  FunctionCallExp* expr;
};

struct VarDeclStatement : public Statement {
  VarDeclStatement(Variable* variable) : variable(variable) {}
  ~VarDeclStatement() override = default;

  StatementType GetType() const override { return StatementType::kVarDecl; }

  Variable* variable;
};

struct KeywordStatement : public Statement {
  explicit KeywordStatement(StatementType type) : type(type) {}
  ~KeywordStatement() override = default;

  StatementType GetType() const override { return type; }

  StatementType type;
};

struct AssignStatement : public Statement {
  AssignStatement(Expression* lhs, Expression* rhs, std::optional<BinaryOp> op);
  ~AssignStatement() override = default;

  StatementType GetType() const override { return StatementType::kAssign; }

  Expression* lhs;
  Expression* rhs;
  std::optional<BinaryOp> op;
};

struct IncrementDeclStatement : public Statement {
  IncrementDeclStatement(Expression* lhs, bool inc);
  ~IncrementDeclStatement() override = default;

  StatementType GetType() const override;

  Expression* lhs;
  bool increment;
};

struct IfStatement : public Statement {
  IfStatement(Expression* condition, BlockStatement* body, Statement* else_stmt,
              std::vector<Attribute*> attributes);
  ~IfStatement() override = default;

  StatementType GetType() const override;

  Expression* condition;
  BlockStatement* body;
  Statement* else_stmt;
  std::vector<Attribute*> attributes;
};

struct CaseSelector : public Node {
  CaseSelector() = default;
  explicit CaseSelector(Expression* expr) : expr(expr) {}

  ~CaseSelector() override = default;

  void Accept(AstVisitor* visitor) override;

  bool IsDefault() const { return expr == nullptr; }

  Expression* expr = nullptr;
};

struct CaseStatement : public Statement {
  CaseStatement(std::vector<CaseSelector*> selectors, BlockStatement* body);
  ~CaseStatement() override = default;

  StatementType GetType() const override;

  std::vector<CaseSelector*> selectors;
  BlockStatement* body;
};

struct SwitchStatement : public Statement {
  SwitchStatement(Expression* condition, std::vector<CaseStatement*> body,
                  std::vector<Attribute*> stmt_attrs,
                  std::vector<Attribute*> body_attrs);

  ~SwitchStatement() override = default;

  StatementType GetType() const override;

  Expression* condition;
  std::vector<CaseStatement*> body;
  std::vector<Attribute*> stmt_attrs;
  std::vector<Attribute*> body_attrs;
};

struct LoopStatement : public Statement {
  LoopStatement(BlockStatement* body, BlockStatement* continuing,
                std::vector<Attribute*> attributes);
  ~LoopStatement() override = default;

  StatementType GetType() const override;

  BlockStatement* body;
  BlockStatement* continuing;
  std::vector<Attribute*> attributes;
};

struct ForLoopStatement : public Statement {
  ForLoopStatement(Statement* initializer, Expression* condition,
                   Statement* continuing, BlockStatement* body,
                   std::vector<Attribute*> attributes);
  ~ForLoopStatement() override = default;

  StatementType GetType() const override;

  Statement* initializer;
  Expression* condition;
  Statement* continuing;
  BlockStatement* body;
  std::vector<Attribute*> attributes;
};

struct WhileLoopStatement : public Statement {
  WhileLoopStatement(Expression* condition, BlockStatement* body,
                     std::vector<Attribute*> attributes);
  ~WhileLoopStatement() override = default;

  StatementType GetType() const override;

  Expression* condition;
  BlockStatement* body;
  std::vector<Attribute*> attributes;
};

struct BreakIfStatement : public Statement {
  explicit BreakIfStatement(Expression* exp);
  ~BreakIfStatement() override = default;

  StatementType GetType() const override;

  Expression* condition;
};

}  // namespace ast
}  // namespace wgx
