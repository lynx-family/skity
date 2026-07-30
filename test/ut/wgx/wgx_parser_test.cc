// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "module/wgx/wgsl/ast/ast_function.h"
#include "module/wgx/wgsl/ast/ast_module.h"
#include "module/wgx/wgsl/ast/expression.h"
#include "module/wgx/wgsl/ast/identifier.h"
#include "module/wgx/wgsl/ast/statement.h"
#include "module/wgx/wgsl/parser.h"
#include "module/wgx/wgsl/scanner.h"

namespace {

using wgx::ast::BlockStatement;
using wgx::ast::Expression;
using wgx::ast::ExpressionType;
using wgx::ast::IdentifierExp;
using wgx::ast::IfStatement;
using wgx::ast::IntLiteralExp;
using wgx::ast::Module;
using wgx::ast::ParenExp;
using wgx::ast::ReturnStatement;
using wgx::ast::Statement;
using wgx::ast::StatementType;

// Peel a top-level parenthesized condition: `if (a)` parses the condition as
// a ParenExp wrapping the identifier.
Expression* UnwrapParen(Expression* expr) {
  if (expr != nullptr && expr->GetType() == ExpressionType::kParenExp) {
    auto* paren = static_cast<ParenExp*>(expr);
    if (!paren->exps.empty()) {
      return paren->exps.front();
    }
  }
  return expr;
}

// Walk the nested else_stmt chain of an if/else-if construct and collect each
// branch's condition identifier name, in source order. Stops at the terminal
// else block (or nullptr).
std::vector<std::string_view> ChainConditionNames(IfStatement* head) {
  std::vector<std::string_view> names;
  Statement* cur = head;
  while (cur != nullptr && cur->GetType() == StatementType::kIf) {
    auto* node = static_cast<IfStatement*>(cur);
    auto* cond = UnwrapParen(node->condition);
    if (cond != nullptr && cond->GetType() == ExpressionType::kIdentifier) {
      names.push_back(static_cast<IdentifierExp*>(cond)->ident->name);
    }
    cur = node->else_stmt;
  }
  return names;
}

// Collect the first statement of each branch body as an int return literal,
// in source order. Used to assert that bodies stay paired with their
// conditions after the chain is assembled.
std::vector<int64_t> ChainBranchReturnValues(IfStatement* head) {
  std::vector<int64_t> values;
  Statement* cur = head;
  while (cur != nullptr && cur->GetType() == StatementType::kIf) {
    auto* node = static_cast<IfStatement*>(cur);
    if (node->body != nullptr && !node->body->statements.empty()) {
      auto* stmt = node->body->statements[0];
      if (stmt->GetType() == StatementType::kReturn) {
        auto* ret = static_cast<ReturnStatement*>(stmt);
        if (ret->value != nullptr &&
            ret->value->GetType() == ExpressionType::kIntLiteral) {
          values.push_back(static_cast<IntLiteralExp*>(ret->value)->value);
        }
      }
    }
    cur = node->else_stmt;
  }
  return values;
}

// Return the statement that terminates the chain: the terminal else block, or
// nullptr when there is no else clause.
Statement* ChainElseClause(IfStatement* head) {
  Statement* cur = head;
  while (cur != nullptr && cur->GetType() == StatementType::kIf) {
    cur = static_cast<IfStatement*>(cur)->else_stmt;
  }
  return cur;
}

// Parser-level tests. These go straight through Scanner + Parser (no resolver)
// so they pin down the raw AST the parser emits.
class WgxParserTest : public ::testing::Test {
 protected:
  // Tokens and identifiers store string_views into the source, so the source
  // string must outlive the module -- keep it as a member.
  Module* Parse(std::string source) {
    source_ = std::move(source);
    tokens_ = wgx::Scanner{source_}.Scan();
    allocator_ = std::make_unique<wgx::ast::NodeAllocator>();
    parser_ = std::make_unique<wgx::Parser>(allocator_.get(), tokens_);
    module_ = parser_->BuildModule();
    return module_;
  }

  // First IfStatement found directly under the named function's body.
  IfStatement* FirstIfIn(const std::string& function_name) {
    if (module_ == nullptr) {
      return nullptr;
    }
    auto* fn = module_->GetFunction(function_name);
    if (fn == nullptr || fn->body == nullptr) {
      return nullptr;
    }
    for (auto* stmt : fn->body->statements) {
      if (stmt->GetType() == StatementType::kIf) {
        return static_cast<IfStatement*>(stmt);
      }
    }
    return nullptr;
  }

  std::string source_;
  std::vector<wgx::Token> tokens_;
  std::unique_ptr<wgx::ast::NodeAllocator> allocator_;
  std::unique_ptr<wgx::Parser> parser_;
  Module* module_ = nullptr;
};

// Regression for the if/else-if branch-order bug: Parser::IfStatement used to
// assemble the nested chain with a forward loop, which reversed the whole
// branch sequence (each condition+body swapped) on every backend.
TEST_F(WgxParserTest, PreservesElseIfChainOrder) {
  Parse(R"(
fn branch(a: bool, b: bool, c: bool) -> i32 {
  if (a) {
    return 1;
  } else if (b) {
    return 2;
  } else if (c) {
    return 3;
  } else {
    return 4;
  }
})");
  ASSERT_NE(module_, nullptr) << parser_->GetDiagnosis().message;

  auto* if_stmt = FirstIfIn("branch");
  ASSERT_NE(if_stmt, nullptr);

  // Conditions must remain in source order a, b, c. Pre-fix this returned
  // c, b, a because the chain was built inside-out via forward iteration.
  EXPECT_EQ(ChainConditionNames(if_stmt),
            (std::vector<std::string_view>{"a", "b", "c"}));

  // Bodies must stay paired with their conditions, proving the swap affected
  // whole branches and not just the condition pointers.
  EXPECT_EQ(ChainBranchReturnValues(if_stmt), (std::vector<int64_t>{1, 2, 3}));

  // The terminal else clause is a non-empty block returning 4.
  auto* else_clause = ChainElseClause(if_stmt);
  ASSERT_NE(else_clause, nullptr);
  ASSERT_EQ(else_clause->GetType(), StatementType::kBlock);
  auto* else_block = static_cast<BlockStatement*>(else_clause);
  ASSERT_FALSE(else_block->statements.empty());
  ASSERT_EQ(else_block->statements[0]->GetType(), StatementType::kReturn);
  auto* ret = static_cast<ReturnStatement*>(else_block->statements[0]);
  ASSERT_NE(ret->value, nullptr);
  ASSERT_EQ(ret->value->GetType(), ExpressionType::kIntLiteral);
  EXPECT_EQ(static_cast<IntLiteralExp*>(ret->value)->value, 4);
}

// A lone `if` with no else must leave else_stmt null rather than fabricating a
// trailing block.
TEST_F(WgxParserTest, IfWithoutElseHasNullElseClause) {
  Parse(R"(
fn single(a: bool) -> i32 {
  if (a) {
    return 1;
  }
  return 0;
})");
  ASSERT_NE(module_, nullptr) << parser_->GetDiagnosis().message;

  auto* if_stmt = FirstIfIn("single");
  ASSERT_NE(if_stmt, nullptr);
  EXPECT_EQ(ChainConditionNames(if_stmt), (std::vector<std::string_view>{"a"}));
  EXPECT_EQ(ChainElseClause(if_stmt), nullptr);
}

// An `if`/`else` (no else-if) must attach the else body directly, with the
// chain collapsing to a single conditional branch.
TEST_F(WgxParserTest, IfWithElseBlockKeepsOrder) {
  Parse(R"(
fn two(a: bool) -> i32 {
  if (a) {
    return 1;
  } else {
    return 2;
  }
})");
  ASSERT_NE(module_, nullptr) << parser_->GetDiagnosis().message;

  auto* if_stmt = FirstIfIn("two");
  ASSERT_NE(if_stmt, nullptr);
  EXPECT_EQ(ChainConditionNames(if_stmt), (std::vector<std::string_view>{"a"}));
  EXPECT_EQ(ChainBranchReturnValues(if_stmt), (std::vector<int64_t>{1}));

  auto* else_clause = ChainElseClause(if_stmt);
  ASSERT_NE(else_clause, nullptr);
  ASSERT_EQ(else_clause->GetType(), StatementType::kBlock);
  auto* else_block = static_cast<BlockStatement*>(else_clause);
  ASSERT_FALSE(else_block->statements.empty());
  ASSERT_EQ(else_block->statements[0]->GetType(), StatementType::kReturn);
  auto* ret = static_cast<ReturnStatement*>(else_block->statements[0]);
  ASSERT_EQ(ret->value->GetType(), ExpressionType::kIntLiteral);
  EXPECT_EQ(static_cast<IntLiteralExp*>(ret->value)->value, 2);
}

}  // namespace
