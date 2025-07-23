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

#include "wgsl/ast/identifier.h"
#include "wgsl/ast/node.h"

namespace wgx {
namespace ast {

enum class UnaryOp {
  kAddressOf,    // &
  kComplement,   // ~
  kIndirection,  // *
  kNegation,     // -
  kNot,          // !
};

enum class BinaryOp {
  kAnd,               // &
  kOr,                // |
  kXor,               // ^
  kLogicalAnd,        // &&
  kLogicalOr,         // ||
  kEqual,             // ==
  kNotEqual,          // !=
  kLessThan,          // <
  kGreaterThan,       // >
  kLessThanEqual,     // <=
  kGreaterThanEqual,  // >=
  kShiftLeft,         // <<
  kShiftRight,        // >>
  kAdd,               // +
  kSubtract,          // -
  kMultiply,          // *
  kDivide,            // /
  kModulo,            // %
};

std::string_view op_to_string(BinaryOp op);

enum class ExpressionType {
  kBoolLiteral,
  kIntLiteral,
  kFloatLiteral,
  kIdentifier,
  kFuncCall,
  kParenExp,
  kUnaryExp,
  kIndexAccessor,
  kMemberAccessor,
  kBinaryExp,
  kPhonyExp,
};

struct Expression : public Node {
  ~Expression() override = default;

  virtual ExpressionType GetType() = 0;

  void Accept(AstVisitor* visitor) override;
};

template <typename V, ExpressionType T>
struct LiteralExpression : public Expression {
  explicit LiteralExpression(V v) : value(v) {}
  ~LiteralExpression() override = default;

  ExpressionType GetType() override { return T; }

  V value;
};

using BoolLiteralExp = LiteralExpression<bool, ExpressionType::kBoolLiteral>;
using FloatLiteralExp =
    LiteralExpression<double, ExpressionType::kFloatLiteral>;
using IntLiteralExp = LiteralExpression<int64_t, ExpressionType::kIntLiteral>;

struct IdentifierExp : public Expression {
  explicit IdentifierExp(Identifier* id);
  ~IdentifierExp() override = default;

  ExpressionType GetType() override;

  Identifier* ident;
};

struct FunctionCallExp : public Expression {
  FunctionCallExp(IdentifierExp* id, std::vector<Expression*> args);
  ~FunctionCallExp() override = default;

  ExpressionType GetType() override;

  IdentifierExp* ident;
  std::vector<Expression*> args;
};

struct ParenExp : public Expression {
  explicit ParenExp(std::vector<Expression*> exps);
  ~ParenExp() override = default;

  ExpressionType GetType() override;

  std::vector<Expression*> exps;
};

struct UnaryExp : public Expression {
  UnaryExp(UnaryOp op, Expression* exp);
  ~UnaryExp() override = default;

  ExpressionType GetType() override;

  UnaryOp op;
  Expression* exp;
};

struct IndexAccessorExp : public Expression {
  IndexAccessorExp(Expression* obj, Expression* idx);
  ~IndexAccessorExp() override = default;

  ExpressionType GetType() override;

  Expression* obj;
  Expression* idx;
};

struct MemberAccessor : public Expression {
  MemberAccessor(Expression* obj, Identifier* member);
  ~MemberAccessor() override = default;

  ExpressionType GetType() override;

  Expression* obj;
  Identifier* member;
};

struct BinaryExp : public Expression {
  BinaryExp(BinaryOp op, Expression* lhs, Expression* rhs);
  ~BinaryExp() override = default;

  ExpressionType GetType() override;

  BinaryOp op;
  Expression* lhs;
  Expression* rhs;
};

struct PhonyExpression : public Expression {
  PhonyExpression() = default;
  ~PhonyExpression() override = default;

  ExpressionType GetType() override { return ExpressionType::kPhonyExp; }
};

}  // namespace ast
}  // namespace wgx
